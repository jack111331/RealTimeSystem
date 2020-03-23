#include <iostream>
#include <chrono>
#include <cmath>

using namespace std::chrono;

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

void setSchedulingPolicy(int policy, int priority) {
  sched_param sched;
  sched_getparam(0, &sched);
  sched.sched_priority = priority;
  if(sched_setscheduler(0, policy, &sched)) {
    perror("sched_setscheduler");
    exit(EXIT_FAILURE);
  }
}

void workload_1ms() {
  int repeat = 130000;
  float a[2] = {100, 0};
  for(int i = 0;i <= repeat;++i) {
    a[(i+1)&1] = sqrt(a[i&1]);
  }
}

int main() {
  int policy = SCHED_FIFO;
  int priority = 99;
  setSchedulingPolicy(policy, priority);

  system_clock::time_point startTime = system_clock::now();
  workload_1ms();
  system_clock::time_point endTime = system_clock::now();
  const int delta = duration_cast<microseconds>(endTime-startTime).count();
  std::cout << "response time of our workload = " << delta << " us" << std::endl;

  return 0;
}
