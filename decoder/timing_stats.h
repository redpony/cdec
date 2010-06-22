#ifndef _TIMING_STATS_H_
#define _TIMING_STATS_H_

#include <string>
#include <map>

struct TimerInfo {
  int calls;
  double total_time;
  TimerInfo() : calls(), total_time() {}
};

struct Timer {
  Timer(const std::string& info);
  ~Timer();
  static void Summarize();
 private:
  static std::map<std::string, TimerInfo> stats;
  clock_t start_t;
  TimerInfo& cur;
  Timer(const Timer& other);
  const Timer& operator=(const Timer& other);
};

#endif
