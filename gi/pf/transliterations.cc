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
  GraphStructure() : initialized(false) {}
  boost::shared_ptr<Reachability> r;
  bool initialized;
};

struct TransliterationsImpl {
  TransliterationsImpl() {
  }

  void Initialize(WordID src, const vector<WordID>& src_lets, WordID trg, const vector<WordID>& trg_lets) {
    const size_t src_len = src_lets.size();
    const size_t trg_len = trg_lets.size();
    if (src_len >= graphs.size()) graphs.resize(src_len + 1);
    if (trg_len >= graphs[src_len].size()) graphs[src_len].resize(trg_len + 1);
    if (graphs[src_len][trg_len].initialized) return;
    graphs[src_len][trg_len].r.reset(new Reachability(src_len, trg_len, 4, 4));

#if 0
    if (HG::Intersect(tlat, &hg)) {
      // TODO
    } else {
      cerr << "No transliteration lattice possible for src_len=" << src_len << " trg_len=" << trg_len << endl;
      hg.clear();
    }
    //cerr << "Number of paths: " << graphs[src][trg].lattice.NumberOfPaths() << endl;
#endif
    graphs[src_len][trg_len].initialized = true;
  }

  void Forbid(WordID src, const vector<WordID>& src_lets, WordID trg, const vector<WordID>& trg_lets) {
    const size_t src_len = src_lets.size();
    const size_t trg_len = trg_lets.size();
    if (src_len >= graphs.size()) graphs.resize(src_len + 1);
    if (trg_len >= graphs[src_len].size()) graphs[src_len].resize(trg_len + 1);
    graphs[src_len][trg_len].r.reset();
    graphs[src_len][trg_len].initialized = true;
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
  }

  vector<vector<GraphStructure> > graphs; // graphs[src_len][trg_len]
};

Transliterations::Transliterations() : pimpl_(new TransliterationsImpl) {}
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

