#include <iostream>
#include <chrono>
#include <cmath>

using namespace std::chrono;

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static sig_atomic_t jobExecuted;
static sig_atomic_t jobMissed;

static void setSchedulingPolicy(int policy, int priority) {
  sched_param sched;
  sched_getparam(0, &sched);
  sched.sched_priority = priority;
  if(sched_setscheduler(0, policy, &sched)) {
    perror("sched_setscheduler");
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

static void collectData(int param) {
  std::cout << static_cast<float>(::jobMissed)/static_cast<float>(::jobExecuted) << std::endl;
}

int main() {
  void (*prevHandler)(int);
  prevHandler = signal(SIGINT, collectData);
  pinCPU(0);
  int period = 7000; //us
  int execTime = 2;
  int policy = SCHED_FIFO;
  int priority = 98;
  setSchedulingPolicy(policy, priority);
  while(1) {
    // Released
    system_clock::time_point startTime = system_clock::now();
    for(int i = 0;i < execTime;++i) {
      workload_1ms();
    }
    // Responsed
    system_clock::time_point endTime = system_clock::now();
    const int delta = duration_cast<microseconds>(endTime-startTime).count();
    ::jobExecuted++;
    if(delta > period) {
      ::jobMissed++;
      continue;
    } else {
      usleep(period-delta);
    }
    // std::cout << "response time of our workload = " << delta << " us" << std::endl;
  }

  return 0;
}
