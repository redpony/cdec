#include <iostream>
#include <tr1/memory>
#include <queue>

#include "sampler.h"

using namespace std;
using namespace tr1;

shared_ptr<MT19937> prng;

struct Particle {
  Particle() : weight(prob_t::One()) {}
  vector<int> states;
  prob_t weight;
  prob_t gamma_last;
};

ostream& operator<<(ostream& os, const Particle& p) {
  os << "[";
  for (int i = 0; i < p.states.size(); ++i) os << p.states[i] << ' ';
  os << "| w=" << log(p.weight) << ']';
  return os;
}

void Rejuvenate(vector<Particle>& pps) {
  SampleSet<prob_t> ss;
  vector<Particle> nps(pps.size());
  for (int i = 0; i < pps.size(); ++i) {
//    cerr << pps[i] << endl;
    ss.add(pps[i].weight);
  }
//  cerr << "REJUVINATING...\n";
  for (int i = 0; i < pps.size(); ++i) {
    nps[i] = pps[prng->SelectSample(ss)];
    nps[i].weight = prob_t(1.0 / pps.size());
//    cerr << nps[i] << endl;
  }
  nps.swap(pps);
//  exit(1);
}

int main(int argc, char** argv) {
  const unsigned particles = 100;
  prng.reset(new MT19937);
  MT19937& rng = *prng;

  // q(a) = 0.8
  // q(b) = 0.8
  // q(c) = 0.4
  SampleSet<double> ssq;
  ssq.add(0.4);
  ssq.add(0.6);
  ssq.add(0);
  double qz = 1;

  // p(a) = 0.2
  // p(b) = 0.8
  vector<double> p(3);
  p[0] = 0.2;
  p[1] = 0.8;
  p[2] = 0;

  vector<int> counts(3);
  int tot = 0;

  vector<Particle> pps(particles);
  SampleSet<prob_t> ppss;
  int LEN = 12;
  int PP = 1;
  while (pps[0].states.size() < LEN) {
    for (int pi = 0; pi < particles; ++pi) {
      Particle& prt = pps[pi];

      bool redo = true;
      const Particle savedp = prt;
      while (redo) {
        redo = false;
        for (int i = 0; i < PP; ++i) {
          int s = rng.SelectSample(ssq);
          double gamma_last = p[s];
          if (!gamma_last) { redo = true; break; }
          double q = ssq[s] / qz;
          prt.states.push_back(s);
          prt.weight *= prob_t(gamma_last / q);
        }
        if (redo) { prt = savedp; continue; }
      }
    }
    Rejuvenate(pps);
  }
  ppss.clear();
  for (int i = 0; i < particles; ++i) { ppss.add(pps[i].weight); }
  int sp = rng.SelectSample(ppss);
  cerr << pps[sp] << endl;

  return 0;
}

