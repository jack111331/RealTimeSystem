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

#include "es.grpc.pb.h"

#include <unistd.h>
#include <sched.h>
#include <cmath>

#include "json.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ServerWriter;
using grpc::ServerReader;

using es::TopicRequest;
using es::EventService;
using es::TopicData;
using es::NoUse;

using nlohmann::json;

pthread_mutex_t mutexHigh;
pthread_mutex_t mutexMid;
pthread_mutex_t mutexLow;
pthread_cond_t cvHigh;
pthread_cond_t cvMid;
pthread_cond_t cvLow;
ServerWriter<TopicData>* writerHigh = NULL;
ServerWriter<TopicData>* writerMid = NULL;
ServerWriter<TopicData>* writerLow = NULL;
TopicData tdHigh, tdMid, tdLow;

pthread_mutex_t mutexPq;

static std::string configFile;

struct Config {
  int period;
  int deadline;
};

static std::map<std::string, Config> configMap;

int arriveTimeCount;

struct MessageMeta {
  int arriveTime;
  int period;
  int deadline;
  TopicData msg;
};

struct EDF {
  bool operator () (const MessageMeta &lhs, const MessageMeta &rhs) const {
    return lhs.deadline < rhs.deadline;
  }
};

struct RM {
  bool operator () (const MessageMeta &lhs, const MessageMeta &rhs) const {
    return lhs.period < rhs.period;
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
      exit(1);
    }
  }
};

static std::priority_queue<MessageMeta, std::vector<MessageMeta>, SchedulingStrategy> *pq = nullptr;

bool atomicHasData() {
  int size = 0;
  pthread_mutex_lock(&mutexPq);
  size = pq->size();
  pthread_mutex_unlock(&mutexPq);
  return size > 0;
}

void atomicPush(const TopicData &topicData) {
  pthread_mutex_lock(&mutexPq);
  MessageMeta msgMeta;
  msgMeta.msg = topicData;
  msgMeta.arriveTime = arriveTimeCount++;
  Config config = configMap[msgMeta.msg.topic()];
  msgMeta.period = config.period;
  msgMeta.deadline = config.deadline;

  pq->push(msgMeta);
  if(topicData.topic() == "High") {
    pthread_cond_broadcast(&cvHigh);
  } else if(topicData.topic() == "Middle") {
    pthread_cond_broadcast(&cvMid);
  } else if(topicData.topic() == "Low") {
    pthread_cond_broadcast(&cvLow);
  }
  pthread_mutex_unlock(&mutexPq);
}

TopicData atomicTop() {
  pthread_mutex_lock(&mutexPq);
  MessageMeta top = pq->top();
  pthread_mutex_unlock(&mutexPq);
  return top.msg;
}

void atomicPop() {
  pthread_mutex_lock(&mutexPq);
  pq->pop();
  pthread_mutex_unlock(&mutexPq);
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

void setSchedulingPolicy (int newPolicy, int priority)
{
    sched_param sched;
    int oldPolicy;
    if (pthread_getschedparam(pthread_self(), &oldPolicy, &sched)) {
        perror("pthread_setschedparam");
        exit(EXIT_FAILURE);
    }
    sched.sched_priority = priority;
    if (pthread_setschedparam(pthread_self(), newPolicy, &sched)) {
        perror("pthread_setschedparam");
        exit(EXIT_FAILURE);
    }
}

void workload_1ms ()
{
    double c = 10.1;
    int repeat = 70000;
    for (int i = 1; i <= repeat; i++)
    {
        c = sqrt(i*i*c);
    }
}

void* highPriorityTask (void *id) {
  std::cout << "Starting the high-priority task...\n";
  setSchedulingPolicy(SCHED_FIFO, 99);
  while (1) {
    pthread_mutex_lock(&mutexHigh);
    while (!atomicHasData() || ((tdHigh = atomicTop()).topic() != "High")) {
      pthread_cond_wait(&cvHigh, &mutexHigh);
    }
    atomicPop();
    if (writerHigh != NULL) {
      workload_1ms ();
      std::cout << "{" << tdHigh.topic() << ": " << tdHigh.data() << "}" << std::endl;
      writerHigh->Write(tdHigh);
    }
    else {
      std::cout << "no subscriber; discard the data\n";
    }
    pthread_mutex_unlock(&mutexHigh);
  }
}

void* midPriorityTask (void *id) {
  std::cout << "Starting the middle-priority task...\n";
  setSchedulingPolicy(SCHED_FIFO, 98);
  while (1) {
    pthread_mutex_lock(&mutexMid);
    while (!atomicHasData() || ((tdMid = atomicTop()).topic() != "Middle")) {
      pthread_cond_wait(&cvMid, &mutexMid);
    }
    atomicPop();
    if (writerMid != NULL) {
      for (int i = 0; i < 10; i++) {
        workload_1ms ();
      }
      std::cout << "{" << tdMid.topic() << ": " << tdMid.data() << "}" << std::endl;
      writerMid->Write(tdMid);
    }
    else {
      std::cout << "no subscriber; discard the data\n";
    }
    pthread_mutex_unlock(&mutexMid);
  }
}

void* lowPriorityTask (void *id) {
  std::cout << "Starting the low-priority task...\n";
  setSchedulingPolicy(SCHED_FIFO, 97);
  while (1) {
    pthread_mutex_lock(&mutexLow);
    while (!atomicHasData() || ((tdLow = atomicTop()).topic() != "Low")) {
      pthread_cond_wait(&cvLow, &mutexLow);
    }
    atomicPop();
    if (writerLow != NULL) {
      for (int i = 0; i < 50; i++) {
        workload_1ms ();
      }
      std::cout << "{" << tdLow.topic() << ": " << tdLow.data() << "}" << std::endl;
      writerLow->Write(tdLow);
    }
    else {
      std::cout << "no subscriber; discard the data\n";
    }
    pthread_mutex_unlock(&mutexLow);
  }
}


// Logic and data behind the server's behavior.
class EventServiceImpl final : public EventService::Service {

 public:
  EventServiceImpl() {
    pinCPU(0);
    pthread_create (&edgeComputing_threads[0], NULL,
                     highPriorityTask, (void *) &idp[0]);
    pthread_create (&edgeComputing_threads[1], NULL,
                     midPriorityTask, (void *) &idp[1]);
    pthread_create (&edgeComputing_threads[2], NULL,
                     lowPriorityTask, (void *) &idp[2]);
    pinCPU(1);
    mutexHigh = PTHREAD_MUTEX_INITIALIZER;
    mutexMid = PTHREAD_MUTEX_INITIALIZER;
    mutexLow = PTHREAD_MUTEX_INITIALIZER;
    cvHigh = PTHREAD_COND_INITIALIZER;
    cvMid = PTHREAD_COND_INITIALIZER;
    cvLow = PTHREAD_COND_INITIALIZER;

    mutexPq = PTHREAD_MUTEX_INITIALIZER;
  }

  Status Subscribe(ServerContext* context,
                   const TopicRequest* request,
                   ServerWriter<TopicData>* writer) override {
  //TODO: use request to determine topic subscription
    writerHigh = writer;
    writerMid = writer;
    writerLow = writer;
    sleep(3600); // preserve the validity of the writer pointer
    return Status::OK;
  }

  Status Publish(ServerContext* context,
                   ServerReader<TopicData>* reader,
                   NoUse* nouse) override {
    TopicData td;
    while (reader->Read(&td)) {
      if (td.topic() != "High" && td.topic() != "Middle" && td.topic() != "Low") {
        std::cerr << "Publish: got an unidentified topic!\n";
      } else {
        atomicPush(td);
      }
    }
    return Status::OK;
  }

 private:
  ServerWriter<TopicData>* writer_ = NULL;
  pthread_t edgeComputing_threads[3];
  const int idp[3] = {0, 1, 2}; // id of each pthread
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
  for(auto topicJson: topicListJson) {
    Config topicConfig = {
      topicJson["Period"],
      topicJson["Deadline"]
    };
    std::cout << topicJson["Name"] << " " << topicConfig.period << " " << topicConfig.deadline << std::endl;
    configMap[topicJson["Name"]] = topicConfig;
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
