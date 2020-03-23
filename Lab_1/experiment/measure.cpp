#include <iostream>
#include <chrono>
using namespace std::chrono;
int main() {
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
