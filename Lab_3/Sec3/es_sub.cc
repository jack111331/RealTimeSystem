/*
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
#include <fstream>

#include <grpcpp/grpcpp.h>

#include "es.grpc.pb.h"

#include <unistd.h>
#include <sys/time.h>

#include <google/protobuf/util/time_util.h>

#include "json.hpp"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::ClientReader;

using es::TopicRequest;
using es::EventService;
using es::TopicData;
using es::NoUse;

using google::protobuf::Timestamp;

using nlohmann::json;

long long int totalTopic = 0;
long long int missTopic = 0;

struct Config {
    int period;
    int deadline;
};

static std::map<std::string, Config> configMap;
std::vector<int> endToEndLatency;

class Subscriber {
 public:
  Subscriber(int pubID, std::shared_ptr<Channel> channel)
      : id_(pubID),
        stub_(EventService::NewStub(channel)) {}

  void Subscribe(TopicRequest request) {
    ClientContext context;
    TopicData td;
    struct timeval tv;
    std::unique_ptr<ClientReader<TopicData> > reader(
        stub_->Subscribe(&context, request));
//    std::cout << "Start to receive data...\n";
    while (reader->Read(&td)) {
     Timestamp currentTimestamp;
      gettimeofday(&tv, NULL);
      currentTimestamp.set_seconds(tv.tv_sec);
      currentTimestamp.set_nanos(tv.tv_usec * 1000);
      long long int durationMilliSecond = google::protobuf::util::TimeUtil::DurationToMilliseconds(currentTimestamp-td.timestamp());
      endToEndLatency.push_back(durationMilliSecond);
//      std::cout << "Publisher-To-Subscriber Latency = " << durationMilliSecond/1000 << " s " << durationMilliSecond%1000 << " ms\n";
      // statistic data
      totalTopic++;
      long long int durationMicroSecond = google::protobuf::util::TimeUtil::DurationToMicroseconds(currentTimestamp-td.timestamp());
      if(durationMicroSecond > configMap[td.topic()].deadline) {
          missTopic++;
      }
    }
    Status status = reader->Finish();
  }

 private:
  std::unique_ptr<EventService::Stub> stub_;
  int id_;
};

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
//        std::cout << topicJson["Name"] << " " << ", Period: " << topicConfig.period << ", Deadline(us): " << topicConfig.deadline << std::endl;
        configMap[topicJson["Name"]] = topicConfig;
    }
}

void signal_handler( int signal_num ) {
    // output: <total topic> <miss topic> <miss rate>
    for(auto i: endToEndLatency) {
        std::cout << i << std::endl;
    }
    std::cout << totalTopic << " " << missTopic << " " << ((double)missTopic)/((double)totalTopic) << std::endl;
    // terminate program
    exit(signal_num);
}

int main(int argc, char** argv) {
  pinCPU(3);
  if (argc != 3) {
      std::cout << "Usage: " << argv[0] << " -c config_file\n";
      exit(0);
  }
  parseConfig(argv[2]);
  signal(SIGABRT, signal_handler);
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  std::string target_str;
  target_str = "localhost:50051";
  Subscriber sub(2, grpc::CreateChannel(
      target_str, grpc::InsecureChannelCredentials()));
  TopicRequest request;
  // Doesn't matter
  request.set_topic("High");
  sub.Subscribe(request);
  sleep(3600);

  return 0;
}
