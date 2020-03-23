#include <iostream>
#include <chrono>

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

int main() {
  int policy = SCHED_FIFO;
  int priority = 99;
  setSchedulingPolicy(policy, priority);

  system_clock::time_point startTime = system_clock::now();
  int a, b;
  for(int i = 0;i < 10000000;++i) {
    a = 10 * 20*b;
    b = 30/40/a;
  }
  int c = 100 / a + b;
  system_clock::time_point endTime = system_clock::now();
  const int delta = duration_cast<microseconds>(endTime-startTime).count();
  std::cout << "response time of our workload = " << delta << " us" << std::endl;
  return 0;
}
