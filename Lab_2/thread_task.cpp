#include <iostream>
#include <chrono>
#include <cmath>
#include <algorithm>

using namespace std::chrono;
using std::sort;

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

struct TaskProperties {
  int period, execTime, priority;
  bool operator < (const TaskProperties &rhs) {
    return period < rhs.period;
  }
};

void setSchedulingPolicy (int newPolicy, int priority) {
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

static void pinCPU(int cpu_number) {
    cpu_set_t mask;
    CPU_ZERO(&mask);

    CPU_SET(cpu_number, &mask);

    if(sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }
}

void *threadTask(void *taskPropertiesPtr) {
  TaskProperties *property = (TaskProperties *)taskPropertiesPtr;
  int period = property->period;
  int execTime = property->execTime;
  int priority = property->priority;
  std::cout << period << " " << execTime << " " << priority << std::endl;
  fflush(stdout);
  setSchedulingPolicy(SCHED_FIFO, priority);
  while(1) {
    // Released
    system_clock::time_point startTime = system_clock::now();
    for(int i = 0;i < execTime;++i) {
      workload_1ms();
    }
    // Responsed
    system_clock::time_point endTime = system_clock::now();
    const int delta = duration_cast<microseconds>(endTime-startTime).count();
    if(delta > period) {
      std::cout << "Task with period = " << period << " and worst case exec time = " << execTime << " exceed period." << std::endl;
      fflush(stdout);
      continue;
    } else {
      std::cout << "response time of task with period = " << period << " and worst case exec time = " << execTime << ", workload = " << delta << " us" << std::endl;
      fflush(stdout);
      usleep(period-delta);
    }
  }
}

static void rateMonotonic(TaskProperties *properties, int taskNum) {
  sort(properties, properties + taskNum);
  for(int i = 0;i < taskNum;++i) {
    properties[i].priority = 99-i;
  }
}

int main() {
  pinCPU(0);
  constexpr int taskNum = 3;
  TaskProperties taskProperties[taskNum] = {
    {50000, 1, 0},
    {200000, 10, 0},
    {100000, 50, 0}
  };
  rateMonotonic(taskProperties, taskNum);
  pthread_t pthread[taskNum];
  for(int i = 0;i < taskNum;++i) {
    if(pthread_create(&pthread[i], NULL, threadTask, (void *)&taskProperties[i]) != 0) {
      perror("pthread_create()");
      exit(1);
    }
  }
  for(int i = 0;i < taskNum;++i) {
    if (pthread_join(pthread[i], NULL) != 0) {
      perror("pthread_join()");
      exit(1);
    }
  }

  return 0;
}
