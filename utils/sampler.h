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
#include <boost/random/gamma_distribution.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/poisson_distribution.hpp>
#include <boost/random/uniform_int.hpp>

#include "prob.h"

template <typename F> class SampleSet;

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

  template <typename F>
  size_t SelectSample(const F& a, const F& b, double T = 1.0) {
    if (T == 1.0) {
      if (F(this->next()) > (a / (a + b))) return 1; else return 0;
    }
    std::cerr << "SelectSample with annealing not implemented\n";
    abort();
    return 0;
  }

  // T is the annealing temperature, if desired
  template <typename F>
  size_t SelectSample(const SampleSet<F>& ss, double T = 1.0);

  // draw a value from U(0,1)
  double next() {return m_random();}

  // draw a value from U(0,1)
  double operator()() { return m_random(); }

  // draw a value from N(mean,var)
  double NextNormal(double mean, double var) {
    return boost::normal_distribution<double>(mean, var)(m_random);
  }

  // draw a value from a Poisson distribution
  // lambda must be greater than 0
  int NextPoisson(int lambda) {
    return boost::poisson_distribution<int>(lambda)(m_random);
  }

  double NextGamma(double shape, double scale = 1.0) {
    boost::gamma_distribution<> gamma(shape);
    boost::variate_generator<boost::mt19937&,boost::gamma_distribution<> > vg(m_generator, gamma);
    return vg() * scale;
  }

  double NextBeta(double alpha, double beta) {
    double x = NextGamma(alpha);
    double y = NextGamma(beta);
    return x / (x + y);
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

template <typename F = double>
class SampleSet {
 public:
  const F& operator[](int i) const { return m_scores[i]; }
  F& operator[](int i) { return m_scores[i]; }
  bool empty() const { return m_scores.empty(); }
  void add(const F& s) { m_scores.push_back(s); }
  void clear() { m_scores.clear(); }
  size_t size() const { return m_scores.size(); }
  void resize(int size) { m_scores.resize(size); }
  std::vector<F> m_scores;
};

template <typename RNG>
template <typename F>
size_t RandomNumberGenerator<RNG>::SelectSample(const SampleSet<F>& ss, double T) {
  assert(T > 0.0);
  assert(ss.m_scores.size() > 0);
  if (ss.m_scores.size() == 1) return 0;
  const double annealing_factor = 1.0 / T;
  const bool anneal = (T != 1.0);
  F sum = F(0);
  if (anneal) {
    for (unsigned i = 0; i < ss.m_scores.size(); ++i)
      sum += pow(ss.m_scores[i], annealing_factor);  // p^(1/T)
  } else {
    sum = std::accumulate(ss.m_scores.begin(), ss.m_scores.end(), F(0));
  }
  //std::cerr << "SUM: " << sum << std::endl;
  //for (size_t i = 0; i < ss.m_scores.size(); ++i) std::cerr << ss.m_scores[i] << ",";
  //std::cerr << std::endl;

  F random(this->next());    // random number between 0 and 1
  random *= sum;                  // scale with normalization factor
  //std::cerr << "Random number " << random << std::endl;

  //now figure out which sample
  size_t position = 1;
  sum = ss.m_scores[0];
  if (anneal) {
    sum = pow(sum, annealing_factor);
    for (; position < ss.m_scores.size() && sum < random; ++position)
      sum += pow(ss.m_scores[position], annealing_factor);
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
