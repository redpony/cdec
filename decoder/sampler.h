#ifndef SAMPLER_H_
#define SAMPLER_H_

#include <algorithm>
#include <functional>
#include <numeric>
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/poisson_distribution.hpp>
#include <boost/random/uniform_int.hpp>

#include "prob.h"

struct SampleSet;

template <typename RNG>
struct RandomNumberGenerator {
  static uint32_t GetTrulyRandomSeed() {
    uint32_t seed;
    std::ifstream r("/dev/urandom");
    if (r) {
      r.read((char*)&seed,sizeof(uint32_t));
    }
    if (r.fail() || !r) {
      std::cerr << "Warning: could not read from /dev/urandom. Seeding from clock" << std::endl;
      seed = std::time(NULL);
    }
    std::cerr << "Seeding random number sequence to " << seed << std::endl;
    return seed;
  }

  RandomNumberGenerator() : m_dist(0,1), m_generator(), m_random(m_generator,m_dist) {
    uint32_t seed = GetTrulyRandomSeed();
    m_generator.seed(seed);
  }
  explicit RandomNumberGenerator(uint32_t seed) : m_dist(0,1), m_generator(), m_random(m_generator,m_dist) {
    if (!seed) seed = GetTrulyRandomSeed();
    m_generator.seed(seed);
  }

  size_t SelectSample(const prob_t& a, const prob_t& b, double T = 1.0) {
    if (T == 1.0) {
      if (this->next() > (a / (a + b))) return 1; else return 0;
    } else {
      assert(!"not implemented");
    }
  }

  // T is the annealing temperature, if desired
  size_t SelectSample(const SampleSet& ss, double T = 1.0);

  // draw a value from U(0,1)
  double next() {return m_random();}

  // draw a value from N(mean,var)
  double NextNormal(double mean, double var) {
    return boost::normal_distribution<double>(mean, var)(m_random);
  }

  // draw a value from a Poisson distribution
  // lambda must be greater than 0
  int NextPoisson(int lambda) {
    return boost::poisson_distribution<int>(lambda)(m_random);
  }

  bool AcceptMetropolisHastings(const prob_t& p_cur,
                                const prob_t& p_prev,
                                const prob_t& q_cur,
                                const prob_t& q_prev) {
    const prob_t a = (p_cur / p_prev) * (q_prev / q_cur);
    if (log(a) >= 0.0) return true;
    return (prob_t(this->next()) < a);
  }

  RNG &gen() { return m_generator; }
  typedef boost::variate_generator<RNG&, boost::uniform_int<> > IntRNG;
  IntRNG inclusive(int low,int high_incl) {
    assert(high_incl>=low);
    return IntRNG(m_generator,boost::uniform_int<>(low,high_incl));
  }

 private:
  boost::uniform_real<> m_dist;
  RNG m_generator;
  boost::variate_generator<RNG&, boost::uniform_real<> > m_random;
};

typedef RandomNumberGenerator<boost::mt19937> MT19937;

class SampleSet {
 public:
  const prob_t& operator[](int i) const { return m_scores[i]; }
  prob_t& operator[](int i) { return m_scores[i]; }
  bool empty() const { return m_scores.empty(); }
  void add(const prob_t& s) { m_scores.push_back(s); }
  void clear() { m_scores.clear(); }
  size_t size() const { return m_scores.size(); }
  void resize(int size) { m_scores.resize(size); }
  std::vector<prob_t> m_scores;
};

template <typename RNG>
size_t RandomNumberGenerator<RNG>::SelectSample(const SampleSet& ss, double T) {
  assert(T > 0.0);
  assert(ss.m_scores.size() > 0);
  if (ss.m_scores.size() == 1) return 0;
  const prob_t annealing_factor(1.0 / T);
  const bool anneal = (annealing_factor != prob_t::One());
  prob_t sum = prob_t::Zero();
  if (anneal) {
    for (int i = 0; i < ss.m_scores.size(); ++i)
      sum += ss.m_scores[i].pow(annealing_factor);  // p^(1/T)
  } else {
    sum = std::accumulate(ss.m_scores.begin(), ss.m_scores.end(), prob_t::Zero());
  }
  //for (size_t i = 0; i < ss.m_scores.size(); ++i) std::cerr << ss.m_scores[i] << ",";
  //std::cerr << std::endl;

  prob_t random(this->next());    // random number between 0 and 1
  random *= sum;                  // scale with normalization factor
  //std::cerr << "Random number " << random << std::endl;

  //now figure out which sample
  size_t position = 1;
  sum = ss.m_scores[0];
  if (anneal) {
    sum.poweq(annealing_factor);
    for (; position < ss.m_scores.size() && sum < random; ++position)
      sum += ss.m_scores[position].pow(annealing_factor);
  } else {
    for (; position < ss.m_scores.size() && sum < random; ++position)
      sum += ss.m_scores[position];
  }
  //std::cout << "random: " << random <<  " sample: " << position << std::endl;
  //std::cerr << "Sample: " << position-1 << std::endl;
  //exit(1);
  return position-1;
}

#endif
