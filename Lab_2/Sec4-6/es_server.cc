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

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

std::string currentSubscribing;
std::map<std::string, TopicData> topicDataMap;
std::map<std::string, bool> isReceivedMap;

void *threadingFunc(void *writerParam) {
  ServerWriter<TopicData> *writer = (ServerWriter<TopicData> *)writerParam;
  while(true) {
    pthread_mutex_lock(&mutex);
    while(!isReceivedMap[currentSubscribing]) {
      pthread_cond_wait(&cond, &mutex);
    }
    writer->Write(topicDataMap[currentSubscribing]);
    isReceivedMap[currentSubscribing] = false;
    pthread_mutex_unlock(&mutex);
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
      return Status::OK;
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
        pthread_mutex_lock(&mutex);
        isReceivedMap[td.topic()] = true;
        pthread_cond_signal(&cond);
        topicDataMap[td.topic()] = td;
        pthread_mutex_unlock(&mutex);
      } else {
        std::cout << "[ERROR] Topic " << td.topic() << " not in supported topic list." << std::endl;
      }
    }
    return Status::OK;
  }
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

int main(int argc, char** argv) {
  RunServer();

  return 0;
}
