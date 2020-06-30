/*
 *
 * Copyright 2020 Chao Wang
 * This code is based on an example from the official gRPC GitHub
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <iostream>
#include <memory>
#include <string>
#include <queue>
#include <map>
#include <fstream>

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <google/protobuf/util/time_util.h>

#include "es.grpc.pb.h"

#include <unistd.h>
#include <sched.h>
#include <sys/time.h>
#include <cmath>

#include "json.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ServerWriter;
using grpc::ServerReader;

using google::protobuf::Timestamp;
using google::protobuf::Duration;

using es::TopicRequest;
using es::EventService;
using es::TopicData;
using es::NoUse;

using nlohmann::json;

const int WORKING_THREAD_POOL = 5;

std::vector<pthread_mutex_t> mutexTempList;
std::vector<pthread_cond_t> cvTempList;
std::vector<ServerWriter<TopicData>*> writerTempList;
std::vector<TopicData> topicDataTempList;
std::vector<bool> isTopicUpdateTempList;
pthread_mutex_t mutexWriter;
std::vector<int> handlerIndexList;

pthread_mutex_t mutexPq;
pthread_mutex_t mutexPqIsntEmpty;
pthread_cond_t cvPqIsntEmpty;

const long long int brokerToSubscriberEstimateTime = 5000; // us
const long long int publisherSentTopicBuffer = 50;
const long long int publisherSentTopicPeriod = 3000;

static std::string configFile;

struct Config {
  int period;
  int deadline;
};

static std::map<std::string, Config> configMap;

long long int arriveTimeCount;

struct MessageMeta {
  long long int arriveTime;
  int period;
  long long int deadline;
  TopicData msg;
};

struct EDF {
  bool operator () (const MessageMeta &lhs, const MessageMeta &rhs) const {
    return lhs.deadline > rhs.deadline;
  }
};

struct RM {
  bool operator () (const MessageMeta &lhs, const MessageMeta &rhs) const {
    return lhs.period > rhs.period;
  }
};

struct FIFO {
  bool operator () (const MessageMeta &lhs, const MessageMeta &rhs) const {
    return lhs.arriveTime > rhs.arriveTime;
  }
};

enum class Strategy {
  STRATEGY_EDF,
  STRATEGY_FIFO,
  STRATEGY_RM,
  STRATEGY_UNDEFINED
};

Strategy getSchedulingStrategy(const std::string &scheduleingString) {
  if(scheduleingString == "EDF") {
    return Strategy::STRATEGY_EDF;
  } else if(scheduleingString == "FIFO") {
    return Strategy::STRATEGY_FIFO;
  } else if(scheduleingString == "RM") {
    return Strategy::STRATEGY_RM;
  } else {
    return Strategy::STRATEGY_UNDEFINED;
  }
}

class SchedulingStrategy {
private:
  Strategy m_schedulingStrategy;
  struct EDF m_edf;
  struct RM m_rm;
  struct FIFO m_fifo;
public:
  void setSchedulingStrategy(Strategy a) {
    m_schedulingStrategy = a;
  }
  bool operator () (const MessageMeta &lhs, const MessageMeta &rhs) const {
    if(m_schedulingStrategy == Strategy::STRATEGY_EDF) {
      return m_edf(lhs, rhs);
    } else if(m_schedulingStrategy == Strategy::STRATEGY_FIFO) {
      return m_fifo(lhs, rhs);
    } else if(m_schedulingStrategy == Strategy::STRATEGY_RM) {
      return m_rm(lhs, rhs);
    } else {
      std::cerr << "No valid Strategy" << std::endl;
      exit(1);
    }
  }
};

static std::priority_queue<MessageMeta, std::vector<MessageMeta>, SchedulingStrategy> *pq = nullptr;

bool atomicPriorityQueueHasData() {
    // has specific rate topic data
  bool result = false;
  pthread_mutex_lock(&mutexPq);
  result = pq->size()>0;
  pthread_mutex_unlock(&mutexPq);
  return result;
}

void atomicPush(TopicData &topicData) {
  Config config = configMap[topicData.topic()];

  struct timeval tv;
  Timestamp sentTopicTime = *topicData.mutable_timestamp();
  Timestamp pushToPrioriQueueTime = Timestamp();
  gettimeofday(&tv, NULL);
  pushToPrioriQueueTime.set_seconds(tv.tv_sec);
  pushToPrioriQueueTime.set_nanos(tv.tv_usec * 1000);
  long long int publisherToPrioriQueueTimeDifference = google::protobuf::util::TimeUtil::DurationToMicroseconds(pushToPrioriQueueTime-sentTopicTime);
  // transform from config deadline to broker schedule deadline, subtract it from its time just before pushed into priority queue, and estimated broker to subscriber time
    pthread_mutex_lock(&mutexPq);
    MessageMeta msgMeta = {
            arriveTimeCount++, // arrive time
            config.period, // period
            0 // deadline, will be calculate later
    };
    msgMeta.msg = topicData;
  msgMeta.deadline = config.deadline - publisherToPrioriQueueTimeDifference - brokerToSubscriberEstimateTime;
//  std::cout << "Config deadline: " << config.deadline << std::endl;
//  std::cout << "Publisher to priority queue delta time: " << pushToPrioriQueueTimeDifference << std::endl;
//  std::cout << "Calculated deadline: " << msgMeta.deadline << std::endl;

  pq->push(msgMeta);
  pthread_mutex_unlock(&mutexPq);
    pthread_mutex_lock(&mutexPqIsntEmpty);
    pthread_cond_broadcast(&cvPqIsntEmpty);
    pthread_mutex_unlock(&mutexPqIsntEmpty);
}

void atomicTopAndPop(int toIndex) {
  pthread_mutex_lock(&mutexPq);
  topicDataTempList[toIndex] = pq->top().msg;
  pq->pop();
  pthread_mutex_unlock(&mutexPq);
  pthread_mutex_lock(&mutexTempList[toIndex]);
  pthread_cond_broadcast(&cvTempList[toIndex]);
  pthread_mutex_unlock(&mutexTempList[toIndex]);
}

bool atomicTopicHasUpdated(int handlerIndex) {
    // has specific rate topic data
    bool result = false;
    pthread_mutex_lock(&mutexTempList[handlerIndex]);
    result = isTopicUpdateTempList[handlerIndex];
    pthread_mutex_unlock(&mutexTempList[handlerIndex]);
    return result;
}

void atomicSetTopicUpdateState(int handlerIndex, bool state) {
    pthread_mutex_lock(&mutexTempList[handlerIndex]);
    isTopicUpdateTempList[handlerIndex] = state;
    pthread_mutex_unlock(&mutexTempList[handlerIndex]);
}


void pinCPU (int cpu_number)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);

    CPU_SET(cpu_number, &mask);

    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1)
    {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }
}

void* sendTopicToSubscriberTask (void *param) {
  pinCPU(0);
  int handlerIndex = *((int *)param);
  std::cout << "Starting the send Topic To Subscriber Task...\n";
  while (1) {
    while (atomicTopicHasUpdated(handlerIndex)) {
        pthread_mutex_lock(&mutexTempList[handlerIndex]);
        pthread_cond_wait(&cvTempList[handlerIndex], &mutexTempList[handlerIndex]);
        pthread_mutex_unlock(&mutexTempList[handlerIndex]);
    }
    if (writerTempList[handlerIndex] != NULL) {
        pthread_mutex_lock(&mutexWriter);
        writerTempList[handlerIndex]->Write(topicDataTempList[handlerIndex]);
        atomicSetTopicUpdateState(handlerIndex, true);
        pthread_mutex_unlock(&mutexWriter);
    } else {
        std::cout << "no subscriber; discard the data\n";
    }
  }
}

void* dispatchQueueTopToTopicTask (void *param) {
    pinCPU(0);
    std::cout << "Starting the dispatch Queue Top To Topic Task...\n";
    // dispatch queue top to topic and let thread pool handle it
    while (1) {
        while (!atomicPriorityQueueHasData()) {
            pthread_mutex_lock(&mutexPqIsntEmpty);
            pthread_cond_wait(&cvPqIsntEmpty, &mutexPqIsntEmpty);
            pthread_mutex_unlock(&mutexPqIsntEmpty);
        }
        for(int i = 0;i < handlerIndexList.size();++i) {
            // Find feasible thread pool to push data and inform it
            if (atomicTopicHasUpdated(i)) {
                atomicSetTopicUpdateState(i, false);
                atomicTopAndPop(i);
                pthread_mutex_lock(&mutexTempList[i]);
                pthread_cond_broadcast(&cvTempList[i]);
                pthread_mutex_unlock(&mutexTempList[i]);
                break;
            }
        }
    }
}


// Logic and data behind the server's behavior.
class EventServiceImpl final : public EventService::Service {

 public:
  EventServiceImpl() {
    pinCPU(0);
    mutexPq = PTHREAD_MUTEX_INITIALIZER;
    mutexWriter = PTHREAD_MUTEX_INITIALIZER;
    mutexPqIsntEmpty = PTHREAD_MUTEX_INITIALIZER;
    cvPqIsntEmpty = PTHREAD_COND_INITIALIZER;
    pthread_create(&dispatcherThreads, NULL, dispatchQueueTopToTopicTask, NULL);

    for(int i = 0;i < handlerIndexList.size();++i) {
        mutexTempList[i] = PTHREAD_MUTEX_INITIALIZER;
        cvTempList[i] = PTHREAD_COND_INITIALIZER;
        edgeComputing_threads.push_back(pthread_t());
        pthread_create(&edgeComputing_threads[i], NULL, sendTopicToSubscriberTask, &handlerIndexList[i]);
    }
    pinCPU(1);
  }

  Status Subscribe(ServerContext* context,
                   const TopicRequest* request,
                   ServerWriter<TopicData>* writer) override {
    std::cout << "Subscriber Channel Created" << std::endl;
    // assign each threads in thread pool a writer to write
    for(auto i : handlerIndexList) {
      writerTempList[i] = writer;
    }
    sleep(3600); // preserve the validity of the writer pointer
    return Status::OK;
  }

  Status Publish(ServerContext* context,
                   ServerReader<TopicData>* reader,
                   NoUse* nouse) override {
    TopicData td;
    std::cout << "Publisher Channel Created" << std::endl;
    while (reader->Read(&td)) {
        atomicPush(td);
    }
    return Status::OK;
  }

 private:
  ServerWriter<TopicData>* writer_ = NULL;
  pthread_t dispatcherThreads;
  std::vector<pthread_t> edgeComputing_threads;
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  EventServiceImpl service;

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

void parseConfig(const std::string &configFilename) {
    /*
    {
      "Topic": [
        {
          "Name":
          "Period":
          "Deadline":
          "SameRateTopicAmount":
        }
      ]
    }
    */
  std::ifstream ifs(configFilename);
  json configJson;
  ifs >> configJson;
  std::vector<json> topicListJson = configJson["Topic"];
  for(int i = 0;i < topicListJson.size();++i) {
      const json topicJson = topicListJson[i];
    Config topicConfig = {
      topicJson["Period"],
      topicJson["Deadline"]
    };
    std::cout << topicJson["Name"] << " " << ", Period: " << topicConfig.period << ", Deadline(us): " << topicConfig.deadline << std::endl;
    configMap[topicJson["Name"]] = topicConfig;

  }
  for(int i = 0;i < WORKING_THREAD_POOL;++i) {
      handlerIndexList.push_back(i);
      writerTempList.push_back(nullptr);
      isTopicUpdateTempList.push_back(true);
      mutexTempList.push_back(pthread_mutex_t());
      cvTempList.push_back(pthread_cond_t());
      topicDataTempList.push_back(TopicData());

  }
}

int main(int argc, char** argv) {
  for(int i = 1;i < argc;++i) {
    std::string cmd(argv[i]);
    if(cmd == "-c") {
      configFile = argv[++i];
      parseConfig(configFile);
    } else if(cmd == "-s") {
      std::string schedulingString = argv[++i];
      SchedulingStrategy ss;
      ss.setSchedulingStrategy(getSchedulingStrategy(schedulingString));
      pq = new std::priority_queue<MessageMeta, std::vector<MessageMeta>, SchedulingStrategy>(ss);
    }
  }
  if(configFile == "" || !pq) {
    std::cerr << "No config file or no scheduling strategy" << std::endl;
    exit(2);
  }
  RunServer();

  return 0;
}
