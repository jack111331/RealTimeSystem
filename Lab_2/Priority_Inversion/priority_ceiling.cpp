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

constexpr int MICRO_SECOND_TO_MILLI_SECOND_FACTOR = 1000;

struct TaskProperties {
  int period, execTime, priority;
  bool isHighOrLowPriority;
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

pthread_mutex_t mutex_section_1;
constexpr int taskNum = 4;
TaskProperties taskProperties[taskNum] = {
  {99, 1, 99, true},
  {100, 50, 98, false},
  {101, 40, 98, false},
  {102, 10, 97, true}
};


void *threadTask(void *taskPropertiesPtr) {
  pinCPU(0);
  setSchedulingPolicy(SCHED_FIFO, ((TaskProperties *)taskPropertiesPtr)->priority);
  TaskProperties *property = (TaskProperties *)taskPropertiesPtr;
  int priority = property->priority;
  int period = property->period;
  int execTime = property->execTime;
  bool isHighOrLowPriority = property->isHighOrLowPriority;
  std::cout << "Current running task's properties is..." << std::endl;
  std::cout << "Period=" << period << ", Worst case execution time=" << execTime << ", Priority=" << priority << std::endl << std::endl;
  fflush(stdout);
  while(1) {
    // Released
    system_clock::time_point startTime = system_clock::now();
    if(isHighOrLowPriority) {
      {
        pthread_mutex_lock(&mutex_section_1);
        for(int i = 0;i < execTime;++i) {
          workload_1ms();
        }
        pthread_mutex_unlock(&mutex_section_1);
      }
    } else {
      for(int i = 0;i < execTime;++i) {
        workload_1ms();
      }
    }
    // Responsed
    system_clock::time_point endTime = system_clock::now();
    const int delta = duration_cast<microseconds>(endTime-startTime).count();
    if(delta > period * MICRO_SECOND_TO_MILLI_SECOND_FACTOR) {
      std::cout << "Task with period = " << period << " and worst case exec time = " << execTime << " exceed period." << std::endl;
      fflush(stdout);
      continue;
    } else {
      std::cout << "response time of task with period = " << period << " and worst case exec time = " << execTime << ", workload = " << delta/1000 << " ms" << std::endl;
      fflush(stdout);
      usleep(period * MICRO_SECOND_TO_MILLI_SECOND_FACTOR-delta);
    }
  }
}

static void rateMonotonic(TaskProperties *properties, int taskNum) {
  sort(properties, properties + taskNum);
  for(int i = 0;i < taskNum;++i) {
    if(i == 0) {
      properties[i].priority = 99;
      properties[i].isHighOrLowPriority = true;
    } else if(i == taskNum-1) {
      properties[i].priority = 97;
      properties[i].isHighOrLowPriority = true;
    } else {
      properties[i].priority = 98;
      properties[i].isHighOrLowPriority = false;
    }
  }
}

int main() {
  pinCPU(0);

  // Setup thread priority ceiling
  // it is applied to the mutex that protects the critical sections
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
  pthread_mutex_init (&mutex_section_1, &mutexattr_prioceiling);

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
