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

std::vector<pthread_mutex_t> mutexTempList;
std::vector<pthread_cond_t> cvTempList;
std::vector<ServerWriter<TopicData>*> writerTempList;
pthread_mutex_t mutexWriter;
std::vector<int> handlerIndexList;

pthread_mutex_t mutexPq;

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
    return lhs.deadline < rhs.deadline;
  }
};

struct RM {
  bool operator () (const MessageMeta &lhs, const MessageMeta &rhs) const {
    return lhs.period > rhs.period;
  }
};

struct FIFO {
  bool operator () (const MessageMeta &lhs, const MessageMeta &rhs) const {
    return lhs.arriveTime < rhs.arriveTime;
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

bool atomicHasData() {
    // has specific rate topic data
  bool result = false;
  pthread_mutex_lock(&mutexPq);
  result = pq->size()>0;
  pthread_mutex_unlock(&mutexPq);
  return result;
}

void atomicPush(TopicData &topicData, int handlerIndex) {
  Config config = configMap[topicData.topic()];
  MessageMeta msgMeta = {
          arriveTimeCount++, // arrive time
          config.period, // period
          0 // deadline, will be calculate later
  };
  msgMeta.msg = topicData;

  struct timeval tv;
  Timestamp sentTopicTime = *topicData.mutable_timestamp();
  Timestamp pushToPrioriQueueTime = Timestamp();
  gettimeofday(&tv, NULL);
  pushToPrioriQueueTime.set_seconds(tv.tv_sec);
  pushToPrioriQueueTime.set_nanos(tv.tv_usec * 1000);
  long long int publisherToPrioriQueueTimeDifference = google::protobuf::util::TimeUtil::DurationToMicroseconds(pushToPrioriQueueTime-sentTopicTime);
  // transform from config deadline to broker schedule deadline, subtract it from its time just before pushed into priority queue, and estimated broker to subscriber time
  msgMeta.deadline = config.deadline - publisherToPrioriQueueTimeDifference - brokerToSubscriberEstimateTime;
//  std::cout << "Config deadline: " << config.deadline << std::endl;
//  std::cout << "Publisher to priority queue delta time: " << pushToPrioriQueueTimeDifference << std::endl;
//  std::cout << "Calculated deadline: " << msgMeta.deadline << std::endl;

  pthread_mutex_lock(&mutexPq);
  pq->push(msgMeta);
  pthread_mutex_unlock(&mutexPq);
  pthread_mutex_lock(&mutexTempList[handlerIndex]);
  pthread_cond_broadcast(&cvTempList[handlerIndex]);
  pthread_mutex_unlock(&mutexTempList[handlerIndex]);
//    std::cout << "Broadcasted" << std::endl;
//    fflush(stdout);

}

TopicData atomicTopAndPop() {
  MessageMeta top;
  pthread_mutex_lock(&mutexPq);
  top = pq->top();
  pq->pop();
  pthread_mutex_unlock(&mutexPq);
  return top.msg;
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

void* priorityTask (void *param) {
  // TODO handle for each thread
  int handlerIndex = *((int *)param);
  std::cout << "Starting the task...\n";
  while (1) {
    while (!atomicHasData()) {
        pthread_mutex_lock(&mutexTempList[handlerIndex]);
        pthread_cond_wait(&cvTempList[handlerIndex], &mutexTempList[handlerIndex]);
        pthread_mutex_unlock(&mutexTempList[handlerIndex]);
    }
    TopicData topicToBeSent = atomicTopAndPop();
    if (writerTempList[handlerIndex] != NULL) {
        pthread_mutex_lock(&mutexWriter);
        writerTempList[handlerIndex]->Write(topicToBeSent);
        pthread_mutex_unlock(&mutexWriter);
    } else {
      std::cout << "no subscriber; discard the data\n";
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

    for(int i = 0;i < handlerIndexList.size();++i) {
        mutexTempList[i] = PTHREAD_MUTEX_INITIALIZER;
        cvTempList[i] = PTHREAD_COND_INITIALIZER;
        edgeComputing_threads.push_back(pthread_t());
        pthread_create (&edgeComputing_threads[i], NULL, priorityTask, &handlerIndexList[i]);
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
    int handlerIndex = currentHandlerIndex++;
    std::cout << "Publisher Channel Created" << std::endl;
    while (reader->Read(&td)) {
        // Push with handler id
        atomicPush(td, handlerIndex);
    }
    return Status::OK;
  }

 private:
  ServerWriter<TopicData>* writer_ = NULL;
  std::vector<pthread_t> edgeComputing_threads;
  int currentHandlerIndex = 0;
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
  std::ifstream ifs(configFilename);
  json configJson;
  ifs >> configJson;
  /*
  {
    "Topic": [
      {
        "Name":
        "Period":
        "Deadline":
      }
    ]
  }
  */
  std::vector<json> topicListJson = configJson["Topic"];
  for(int i = 0;i < topicListJson.size();++i) {
      const json topicJson = topicListJson[i];
    Config topicConfig = {
      topicJson["Period"],
      topicJson["Deadline"]
    };
    std::cout << topicJson["Name"] << " " << ", Period: " << topicConfig.period << ", Deadline(us): " << topicConfig.deadline << std::endl;
    configMap[topicJson["Name"]] = topicConfig;

    mutexTempList.push_back(pthread_mutex_t());
    cvTempList.push_back(pthread_cond_t());
    writerTempList.push_back(nullptr);
      handlerIndexList.push_back(i);
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
