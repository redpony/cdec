#include "timing_stats.h"

#include <iostream>
#include "time.h" //cygwin needs

#include "verbose.h"

using namespace std;

map<string, TimerInfo> Timer::stats;

Timer::Timer(const string& timername) : start_t(clock()), cur(stats[timername]) {}

Timer::~Timer() {
  ++cur.calls;
  const clock_t end_t = clock();
  const double elapsed = (end_t - start_t) / 1000000.0;
  cur.total_time += elapsed;
}

void Timer::Summarize() {
  if (!SILENT) {
    for (map<string, TimerInfo>::iterator it = stats.begin(); it != stats.end(); ++it) {
      cerr << it->first << ": " << it->second.total_time << " secs (" << it->second.calls << " calls)\n";
    }
  }
  stats.clear();
}

