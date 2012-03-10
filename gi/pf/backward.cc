#include "backward.h"

#include <queue>
#include <utility>

#include "array2d.h"
#include "reachability.h"
#include "base_distributions.h"

using namespace std;

BackwardEstimator::BackwardEstimator(const string& s2t,
                    const string& t2s) : m1(new Model1(s2t)), m1inv(new Model1(t2s)) {}

BackwardEstimator::~BackwardEstimator() {
  delete m1; m1 = NULL;
  delete m1inv; m1inv = NULL;
}

float BackwardEstimator::ComputeBackwardProb(const std::vector<WordID>& src,
                                             const std::vector<WordID>& trg,
                                             unsigned src_covered,
                                             unsigned trg_covered,
                                             double s2t_ratio) const {
  if (src_covered == src.size() || trg_covered == trg.size()) {
    assert(src_covered == src.size());
    assert(trg_covered == trg.size());
    return 0;
  }
  static const WordID kNULL = TD::Convert("<eps>");
  const prob_t uniform_alignment(1.0 / (src.size() - src_covered + 1));
  // TODO factor in expected length ratio
  prob_t e; e.logeq(Md::log_poisson(trg.size() - trg_covered, (src.size() - src_covered) * s2t_ratio)); // p(trg len remaining | src len remaining)
  for (unsigned j = trg_covered; j < trg.size(); ++j) {
    prob_t p = (*m1)(kNULL, trg[j]) + prob_t(1e-12);
    for (unsigned i = src_covered; i < src.size(); ++i)
      p += (*m1)(src[i], trg[j]);
    if (p.is_0()) {
      cerr << "ERROR: p(" << TD::Convert(trg[j]) << " | " << TD::GetString(src) << ") = 0!\n";
      assert(!"failed");
    }
    p *= uniform_alignment;
    e *= p;
  }
  // TODO factor in expected length ratio
  const prob_t inv_uniform(1.0 / (trg.size() - trg_covered + 1.0));
  prob_t inv;
  inv.logeq(Md::log_poisson(src.size() - src_covered, (trg.size() - trg_covered) / s2t_ratio));
  for (unsigned i = src_covered; i < src.size(); ++i) {
    prob_t p = (*m1inv)(kNULL, src[i]) + prob_t(1e-12);
    for (unsigned j = trg_covered; j < trg.size(); ++j)
      p += (*m1inv)(trg[j], src[i]);
    if (p.is_0()) {
      cerr << "ERROR: p_inv(" << TD::Convert(src[i]) << " | " << TD::GetString(trg) << ") = 0!\n";
      assert(!"failed");
    }
    p *= inv_uniform;
    inv *= p;
  }
  return (log(e) + log(inv)) / 2;
}

void BackwardEstimator::InitializeGrid(const vector<WordID>& src,
                      const vector<WordID>& trg,
                      const Reachability& r,
                      double s2t_ratio,
                      float* grid) const {
  queue<pair<int,int> > q;
  q.push(make_pair(0,0));
  Array2D<bool> done(src.size()+1, trg.size()+1, false);
  //cerr << TD::GetString(src) << " ||| " << TD::GetString(trg) << endl;
  while(!q.empty()) {
    const pair<int,int> n = q.front();
    q.pop();
    if (done(n.first,n.second)) continue;
    done(n.first,n.second) = true;

    float lp = ComputeBackwardProb(src, trg, n.first, n.second, s2t_ratio);
    if (n.first == 0 && n.second == 0) grid[0] = lp;
    //cerr << "  " << n.first << "," << n.second << "\t" << lp << endl;

    if (n.first == src.size() || n.second == trg.size()) continue;
    const vector<pair<short,short> >& edges = r.valid_deltas[n.first][n.second];
    for (int i = 0; i < edges.size(); ++i)
      q.push(make_pair(n.first + edges[i].first, n.second + edges[i].second));
  }
  //static int cc = 0; ++cc; if (cc == 80) exit(1);
}

