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
#include <vector>

#include <grpcpp/grpcpp.h>
#include <google/protobuf/util/time_util.h>

#include "es.grpc.pb.h"

#include <sys/time.h>
#include <unistd.h>

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
    char name[50];
    int period;
    int deadline;
    std::vector<Timestamp> sameRateLatestTopicArrivalList;
};

static std::map<std::string, Config> configMap;
std::vector<pthread_t> publisherThreadList;

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

void* publisherTask(void *param) {
    std::cout << "Starting the task...\n";
    std::string target_str;
    target_str = "localhost:50051";
    Publisher pub(1, grpc::CreateChannel(
            target_str, grpc::InsecureChannelCredentials()));
    std::cout << "Start to send data to our server..\n";
    std::string topicName = (const char *)param;
    while(1) {
        for(int i = 0;i < configMap[topicName].sameRateLatestTopicArrivalList.size();++i) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            Timestamp currentTp;
            currentTp.set_seconds(tv.tv_sec);
            currentTp.set_nanos(tv.tv_usec * 1000);

            if(google::protobuf::util::TimeUtil::DurationToMilliseconds(currentTp - configMap[topicName].sameRateLatestTopicArrivalList[i]) > configMap[topicName].period) {
                TopicData td;
                td.set_topic(topicName);
                td.set_data("Hello World");
                gettimeofday(&tv, NULL);
                Timestamp *tp = td.mutable_timestamp();
                // *td.mutable_timestamp() = google::protobuf::util::TimeUtil::GetCurrentTime();
                tp->set_seconds(tv.tv_sec);
                tp->set_nanos(tv.tv_usec * 1000);
                configMap[topicName].sameRateLatestTopicArrivalList[i] = *tp;
                pub.Publish(td);
//                std::cout << "Published Data" << std::endl;
//                fflush(stdout);
            }
        }
    }
    pub.done();
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
          "SameRateTopicAmount":
        }
      ]
    }
    */
    std::vector<json> topicListJson = configJson["Topic"];
    for(auto topicJson: topicListJson) {
        Config topicConfig = {
                "",
                topicJson["Period"],
                topicJson["Deadline"]
        };
        int sameRateTopicAmount = topicJson["SameRateTopicAmount"];
        for(int i = 0;i < sameRateTopicAmount;++i) {
            topicConfig.sameRateLatestTopicArrivalList.push_back(Timestamp());
            topicConfig.sameRateLatestTopicArrivalList[i].set_seconds(0);
            topicConfig.sameRateLatestTopicArrivalList[i].set_nanos(0);
        }
        configMap[topicJson["Name"]] = topicConfig;
        strncpy(configMap[topicJson["Name"]].name, topicJson["Name"].get<std::string>().c_str(), topicJson["Name"].get<std::string>().size());
        std::cout << topicJson["Name"] << " " << ", Period: " << topicConfig.period << ", Deadline(us): " << topicConfig.deadline << ", Amount: " << sameRateTopicAmount << std::endl;
    }
    for(auto it = configMap.begin();it != configMap.end();++it) {
        pthread_t newThread;
        pthread_create(&newThread, NULL, publisherTask, it->second.name);
        publisherThreadList.push_back(newThread);
    }
}

int main(int argc, char** argv) {
  pinCPU(2);
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " -c config_file\n";
    exit(0);
  }
  parseConfig(argv[2]);

  while(1);

  return 0;
}
