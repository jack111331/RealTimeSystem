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
#include <sys/time.h>

#include <google/protobuf/util/time_util.h>

#include "json.hpp"


using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::ClientWriter;

using es::TopicRequest;
using es::EventService;
using es::TopicData;
using es::NoUse;

using google::protobuf::Timestamp;

using nlohmann::json;

struct Config {
    int period;
    int deadline;
    int amount;
};

static std::map<std::string, Config> configMap;
int eachRateAmount[3];
int totalTopicAmount;
std::string topicName[3] = {"High", "Middle", "Low"};
TopicData latestPublishedTopicData[3];

class Publisher {
 public:
  Publisher(int pubID, std::shared_ptr<Channel> channel)
      : id_(pubID),
        stub_(EventService::NewStub(channel)),
        writer_(stub_->Publish(&context_, &nouse_)) {}

  void Publish(const TopicData td) {
    if (!writer_->Write(td)) {
      // Broken stream.
      std::cout << "rpc failed: broken stream." << std::endl;
      writer_->WritesDone();
      Status status = writer_->Finish();
      if (status.ok()) {
        std::cout << "Finished.\n";
      } else {
        std::cout << "RecordRoute rpc failed." << std::endl;
      }
      exit(0);
    }
    else {
//      std::cout << "sent {" << td.topic() << ": " << td.data() << "}" << std::endl;
    }
  }

  void done() {
    writer_->WritesDone();
    Status status = writer_->Finish();
    if (status.ok()) {
      std::cout << "Finished.\n";
    } else {
      std::cout << "RecordRoute rpc failed." << std::endl;
    }
  }

 private:
  std::unique_ptr<EventService::Stub> stub_;
  int id_;
  ClientContext context_;
  NoUse nouse_;
  std::unique_ptr<ClientWriter<TopicData> > writer_;
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
          "Amount":
        }
      ]
    }
    */
    std::vector<json> topicListJson = configJson["Topic"];
    for(auto topicJson: topicListJson) {
        Config topicConfig = {
                topicJson["Period"],
                topicJson["Deadline"],
                topicJson["Amount"]
        };
        std::cout << topicJson["Name"] << " " << ", Period: " << topicConfig.period << ", Deadline(us): " << topicConfig.deadline << ", Amount: " << topicConfig.amount << std::endl;
        configMap[topicJson["Name"]] = topicConfig;
        if(topicJson["Name"] == "High") {
            eachRateAmount[0] = topicConfig.amount;
            totalTopicAmount += topicConfig.amount;
        } else if(topicJson["Name"] == "Low") {
            eachRateAmount[2] = topicConfig.amount;
            totalTopicAmount += topicConfig.amount;
        }
    }
}

int main(int argc, char** argv) {
  pinCPU(2);
  if (argc != 5) {
    std::cout << "Usage: " << argv[0] << " -l load -c config.json\n";
    exit(0);
  }
  std::string target_str;
  target_str = "localhost:50051";
  Publisher pub(1, grpc::CreateChannel(
      target_str, grpc::InsecureChannelCredentials()));
  std::cout << "Start to send data to our server..\n";
  parseConfig(argv[4]);
  eachRateAmount[1] = atoi(argv[2]);
  totalTopicAmount += eachRateAmount[1];
  for(int i = 0;i < 3;++i) {
      latestPublishedTopicData[i].mutable_timestamp()->set_seconds(0);
      latestPublishedTopicData[i].mutable_timestamp()->set_nanos(0);
  }
  for(int i = 0;i < totalTopicAmount;++i) {
      int randomValue = rand()%3;

      struct timeval tv;
      gettimeofday(&tv, NULL);
      Timestamp currentTp;
      currentTp.set_seconds(tv.tv_sec);
      currentTp.set_nanos(tv.tv_usec * 1000);

      while(!eachRateAmount[randomValue] || google::protobuf::util::TimeUtil::DurationToMicroseconds(currentTp - *(latestPublishedTopicData[randomValue].mutable_timestamp())) <= configMap[topicName[randomValue]].period) {
          randomValue = (randomValue + 1) % 3;
          gettimeofday(&tv, NULL);
          currentTp.set_seconds(tv.tv_sec);
          currentTp.set_nanos(tv.tv_usec * 1000);
      }

      eachRateAmount[randomValue]--;
      TopicData td;
      td.set_topic(topicName[randomValue]);
      td.set_data("Hello World");
      gettimeofday(&tv, NULL);
      Timestamp *tp = td.mutable_timestamp();
      // *td.mutable_timestamp() = google::protobuf::util::TimeUtil::GetCurrentTime();
      tp->set_seconds(tv.tv_sec);
      tp->set_nanos(tv.tv_usec * 1000);
      latestPublishedTopicData[randomValue] = td;
      pub.Publish(td);
  }
  pub.done();

  return 0;
}
