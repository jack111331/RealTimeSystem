/*
 * Copyright 2020 Chao Wang
 * This code is based on an example from the official gRPC GitHub
 *
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
#include <map>

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include "es.grpc.pb.h"

#include <unistd.h>
#include <sched.h>
#include <cmath>
#include <pthread.h>

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

std::map<std::string, int> priority_mapping = {
  {"H", 0},
  {"M", 1},
  {"L", 2},
};

int schedule_priority[3] = {99, 98, 97};
int worst_case_exec_time[3] = {1, 10, 50};

static void pinCPU(int cpu_number) {
    cpu_set_t mask;
    CPU_ZERO(&mask);

    CPU_SET(cpu_number, &mask);

    if(sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }
}

static void setSchedulingPolicy (int newPolicy, int priority) {
    sched_param sched;
    int oldPolicy;
    if (pthread_getschedparam(pthread_self(), &oldPolicy, &sched)) {
        perror("pthread_getschedparam()");
        exit(EXIT_FAILURE);
    }
    sched.sched_priority = priority;
    if (pthread_setschedparam(pthread_self(), newPolicy, &sched)) {
        perror("pthread_setschedparam()");
        exit(EXIT_FAILURE);
    }
}

static void workload_1ms() {
  int repeat = 130000;
  float a[2] = {100, 0};
  for(int i = 0;i <= repeat;++i) {
    a[(i+1)&1] = sqrt(a[i&1]);
  }
}

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex[3] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};

std::string currentSubscribing;
std::map<std::string, TopicData> topicDataMap;
std::map<std::string, bool> isReceivedMap;

void *threadingFunc(void *writerParam) {
  pinCPU(0);
  ServerWriter<TopicData> *writer = (ServerWriter<TopicData> *)writerParam;
  int execTime = schedule_priority[priority_mapping[currentSubscribing]];
  setSchedulingPolicy(SCHED_FIFO, schedule_priority[priority_mapping[currentSubscribing]]);
  execTime = worst_case_exec_time[priority_mapping[currentSubscribing]];

  while(true) {
    pthread_mutex_lock(&mutex[priority_mapping[currentSubscribing]]);
    while(!isReceivedMap[currentSubscribing]) {
      pthread_cond_wait(&cond, &mutex[priority_mapping[currentSubscribing]]);
    }
    for(int i = 0;i < execTime;++i) {
      workload_1ms();
    }
    writer->Write(topicDataMap[currentSubscribing]);
    isReceivedMap[currentSubscribing] = false;
    pthread_mutex_unlock(&mutex[priority_mapping[currentSubscribing]]);
  }
  return NULL;
}

// Logic and data behind the server's behavior.
class EventServiceImpl final : public EventService::Service {

 public:
  Status Subscribe(ServerContext* context,
                   const TopicRequest* request,
                   ServerWriter<TopicData>* writer) override {
    //TODO
    if(request->topic() == "H" || request->topic() == "M" || request->topic() == "L") {
      if(isReceivedMap.find(request->topic()) == isReceivedMap.end()) {
        isReceivedMap[request->topic()] = false;
      }
    } else {
      std::cout << "[ERROR] Topic " << request->topic() << " not in supported topic list." << std::endl;
      return Status::CANCELLED;
    }
    currentSubscribing = request->topic();
    pthread_t thread;
    if(pthread_create(&thread, NULL, threadingFunc, writer)) {
      std::cout << "[ERROR] pthread creation failed" << std::endl;
      return Status::CANCELLED;
    }
    if(pthread_join(thread, NULL)) {
      std::cout << "[ERROR] pthread join failed" << std::endl;
      return Status::CANCELLED;
    }

    return Status::OK;
  }

  Status Publish(ServerContext* context,
                   ServerReader<TopicData>* reader,
                   NoUse* nouse) override {
    // TODO
    TopicData td;
    while(reader->Read(&td)) {
      std::cout << "received {" << td.topic() << ": " << td.data() << "}" << std::endl;
      if(td.topic() == "H" || td.topic() == "M" || td.topic() == "L") {
        pthread_mutex_lock(&mutex[priority_mapping[td.topic()]]);
        isReceivedMap[td.topic()] = true;
        pthread_cond_signal(&cond);
        topicDataMap[td.topic()] = td;
        pthread_mutex_unlock(&mutex[priority_mapping[td.topic()]]);
      } else {
        std::cout << "[ERROR] Topic " << td.topic() << " not in supported topic list." << std::endl;
      }
    }
    return Status::OK;
  }
};

void RunServer() {
  // Setup pthread priority ceiling protocal
  pthread_mutexattr_t mutexattr_prioceiling;
  int mutex_protocol, high_prio;
  high_prio = sched_get_priority_max(SCHED_FIFO);
  pthread_mutexattr_init(&mutexattr_prioceiling);
  pthread_mutexattr_getprotocol(&mutexattr_prioceiling,
                                 &mutex_protocol);
  pthread_mutexattr_setprotocol(&mutexattr_prioceiling,
                                 PTHREAD_PRIO_PROTECT);
  pthread_mutexattr_setprioceiling(&mutexattr_prioceiling,
                                    high_prio);
  for(int i = 0;i < 3;++i) {
    pthread_mutex_init (&mutex[i], &mutexattr_prioceiling);
  }


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

int main(int argc, char** argv) {
  RunServer();

  return 0;
}
