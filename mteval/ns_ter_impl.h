#ifndef _NS_TER_IMPL_H_
#define _NS_TER_IMPL_H_

#include <vector>
#include <unordered_map>
#include <set>
#include <boost/functional/hash.hpp>

typedef int WordID;

namespace NewScorer {

struct Shift {
  unsigned int d_;
  Shift() : d_() {}
  Shift(int b, int e, int m) : d_() {
    begin(b);
    end(e);
    moveto(m);
  }
  inline int begin() const {
    return d_ & 0x3ff;
  }
  inline int end() const {
    return (d_ >> 10) & 0x3ff;
  }
  inline int moveto() const {
    int m = (d_ >> 20) & 0x7ff;
    if (m > 1024) { m -= 1024; m *= -1; }
    return m;
  }
  inline void begin(int b) {
    d_ &= 0xfffffc00u;
    d_ |= (b & 0x3ff);
  }
  inline void end(int e) {
    d_ &= 0xfff003ffu;
    d_ |= (e & 0x3ff) << 10;
  }
  inline void moveto(int m) {
    bool neg = (m < 0);
    if (neg) { m *= -1; m += 1024; }
    d_ &= 0xfffff;
    d_ |= (m & 0x7ff) << 20;
  }
};

class TERScorerImpl {

 public:
  enum TransType { MATCH, SUBSTITUTION, INSERTION, DELETION };

  explicit TERScorerImpl(const std::vector<WordID>& ref);

  float Calculate(const std::vector<WordID>& hyp,
                  int* subs, int* ins, int* dels, int* shifts) const;

  inline int GetRefLength() const { return ref_.size(); }

 private:
  const std::vector<WordID>& ref_;
  std::set<WordID> rwexists_;

  typedef std::unordered_map<std::vector<WordID>,
          std::set<int>,
          boost::hash<std::vector<WordID>>> NgramToIntsMap;
  mutable NgramToIntsMap nmap_;

  static float MinimumEditDistance(
      const std::vector<WordID>& hyp,
      const std::vector<WordID>& ref,
      std::vector<TransType>* path);

  void BuildWordMatches(const std::vector<WordID>& hyp,
                        NgramToIntsMap* nmap) const;

  static void PerformShift(const std::vector<WordID>& in,
                           int start, int end, int moveto,
                           std::vector<WordID>* out);

  void GetAllPossibleShifts(const std::vector<WordID>& hyp,
                            const std::vector<int>& ralign,
                            const std::vector<bool>& herr,
                            const std::vector<bool>& rerr,
                            const int min_size,
                            std::vector<std::vector<Shift>>* shifts) const;

  bool CalculateBestShift(const std::vector<WordID>& cur,
                          const std::vector<WordID>& /*hyp*/,
                          float curerr,
                          const std::vector<TransType>& path,
                          std::vector<WordID>* new_hyp,
                          float* newerr,
                          std::vector<TransType>* new_path) const;

  static void GetPathStats(const std::vector<TransType>& path,
                           int* subs, int* ins, int* dels);

  float CalculateAllShifts(const std::vector<WordID>& hyp,
                           int* subs, int* ins, int* dels, int* shifts) const;
};

}  // namespace NewScorer

#endif
