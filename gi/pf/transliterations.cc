#include "transliterations.h"

#include <iostream>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "filelib.h"
#include "ccrp.h"
#include "m.h"
#include "reachability.h"

using namespace std;
using namespace std::tr1;

struct GraphStructure {
  GraphStructure() : r() {}
  // leak memory - these are basically static
  const Reachability* r;
  bool IsReachable() const { return r->nodes > 0; }
};

struct BackwardEstimates {
  BackwardEstimates() : gs(), backward() {}
  explicit BackwardEstimates(const GraphStructure& g) :
      gs(&g), backward() {
    if (g.r->nodes > 0)
      backward = new float[g.r->nodes];
  }
  // leak memory, these are static

  // returns an estimate of the marginal probability
  double MarginalEstimate() const {
    if (!backward) return 0;
    return backward[0];
  }

  // returns an backward estimate
  double operator()(int src_covered, int trg_covered) const {
    if (!backward) return 0;
    int ind = gs->r->node_addresses[src_covered][trg_covered];
    if (ind < 0) return 0;
    return backward[ind];
  }
 private:
  const GraphStructure* gs;
  float* backward;
};

struct TransliterationsImpl {
  TransliterationsImpl(int max_src, int max_trg, double fr) :
      kMAX_SRC_CHUNK(max_src),
      kMAX_TRG_CHUNK(max_trg),
      kFILTER_RATIO(fr),
      tot_pairs(), tot_mem() {
  }

  void Initialize(WordID src, const vector<WordID>& src_lets, WordID trg, const vector<WordID>& trg_lets) {
    const size_t src_len = src_lets.size();
    const size_t trg_len = trg_lets.size();

    // init graph structure
    if (src_len >= graphs.size()) graphs.resize(src_len + 1);
    if (trg_len >= graphs[src_len].size()) graphs[src_len].resize(trg_len + 1);
    GraphStructure& gs = graphs[src_len][trg_len];
    if (!gs.r)
      gs.r = new Reachability(src_len, trg_len, kMAX_SRC_CHUNK, kMAX_TRG_CHUNK, kFILTER_RATIO);
    const Reachability& r = *gs.r;

    // init backward estimates
    if (src >= bes.size()) bes.resize(src + 1);
    unordered_map<WordID, BackwardEstimates>::iterator it = bes[src].find(trg);
    if (it != bes[src].end()) return; // already initialized

    it = bes[src].insert(make_pair(trg, BackwardEstimates(gs))).first;
    BackwardEstimates& b = it->second;
    if (!gs.r->nodes) return;  // not derivable subject to length constraints

    // TODO
    tot_pairs++;
    tot_mem += sizeof(float) * gs.r->nodes;
  }

  void Forbid(WordID src, const vector<WordID>& src_lets, WordID trg, const vector<WordID>& trg_lets) {
    const size_t src_len = src_lets.size();
    const size_t trg_len = trg_lets.size();
    // TODO
  }

  prob_t EstimateProbability(WordID s, const vector<WordID>& src, WordID t, const vector<WordID>& trg) const {
    assert(src.size() < graphs.size());
    const vector<GraphStructure>& tv = graphs[src.size()];
    assert(trg.size() < tv.size());
    const GraphStructure& gs = tv[trg.size()];
    // TODO: do prob
    return prob_t::Zero();
  }

  void GraphSummary() const {
    double to = 0;
    double tn = 0;
    double tt = 0;
    for (int i = 0; i < graphs.size(); ++i) {
      const vector<GraphStructure>& vt = graphs[i];
      for (int j = 0; j < vt.size(); ++j) {
        const GraphStructure& gs = vt[j];
        if (!gs.r) continue;
        tt++;
        for (int k = 0; k < i; ++k) {
          for (int l = 0; l < j; ++l) {
            size_t c = gs.r->valid_deltas[k][l].size();
            if (c) {
              tn += 1;
              to += c;
            }
          }
        }
      }
    }
    cerr << "     Average nodes = " << (tn / tt) << endl;
    cerr << "Average out-degree = " << (to / tn) << endl;
    cerr << " Unique structures = " << tt << endl;
    cerr << "      Unique pairs = " << tot_pairs << endl;
    cerr << "          BEs size = " << (tot_mem / (1024.0*1024.0)) << " MB" << endl;
  }

  const int kMAX_SRC_CHUNK;
  const int kMAX_TRG_CHUNK;
  const double kFILTER_RATIO;
  unsigned tot_pairs;
  size_t tot_mem;
  vector<vector<GraphStructure> > graphs; // graphs[src_len][trg_len]
  vector<unordered_map<WordID, BackwardEstimates> > bes; // bes[src][trg]
};

Transliterations::Transliterations(int max_src, int max_trg, double fr) :
    pimpl_(new TransliterationsImpl(max_src, max_trg, fr)) {}
Transliterations::~Transliterations() { delete pimpl_; }

void Transliterations::Initialize(WordID src, const vector<WordID>& src_lets, WordID trg, const vector<WordID>& trg_lets) {
  pimpl_->Initialize(src, src_lets, trg, trg_lets);
}

prob_t Transliterations::EstimateProbability(WordID s, const vector<WordID>& src, WordID t, const vector<WordID>& trg) const {
  return pimpl_->EstimateProbability(s, src,t, trg);
}

void Transliterations::Forbid(WordID src, const vector<WordID>& src_lets, WordID trg, const vector<WordID>& trg_lets) {
  pimpl_->Forbid(src, src_lets, trg, trg_lets);
}

void Transliterations::GraphSummary() const {
  pimpl_->GraphSummary();
}

