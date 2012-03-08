#include "transliterations.h"

#include <iostream>
#include <vector>
#include <tr1/unordered_map>

#include "grammar.h"
#include "bottom_up_parser.h"
#include "hg.h"
#include "hg_intersect.h"
#include "filelib.h"
#include "ccrp.h"
#include "m.h"
#include "lattice.h"
#include "verbose.h"

using namespace std;
using namespace std::tr1;

static WordID kX;
static int kMAX_SRC_SIZE = 0;
static vector<vector<WordID> > cur_trg_chunks;

vector<GrammarIter*> tlttofreelist;

static void InitTargetChunks(int max_size, const vector<WordID>& trg) {
  cur_trg_chunks.clear();
  vector<WordID> tmp;
  unordered_set<vector<WordID>, boost::hash<vector<WordID> > > u;
  for (int len = 1; len <= max_size; ++len) {
    int end = trg.size() + 1;
    end -= len;
    for (int i = 0; i < end; ++i) {
      tmp.clear();
      for (int j = 0; j < len; ++j)
        tmp.push_back(trg[i + j]);
      if (u.insert(tmp).second) cur_trg_chunks.push_back(tmp);
    }
  }
}

struct TransliterationGrammarIter : public GrammarIter, public RuleBin {
  TransliterationGrammarIter() { tlttofreelist.push_back(this); }
  TransliterationGrammarIter(const TRulePtr& inr, int symbol) {
    if (inr) {
      r.reset(new TRule(*inr));
    } else {
      r.reset(new TRule);
    }
    TRule& rr = *r;
    rr.lhs_ = kX;
    rr.f_.push_back(symbol);
    tlttofreelist.push_back(this);
  }
  virtual int GetNumRules() const {
    if (!r) return 0;
    return cur_trg_chunks.size();
  }
  virtual TRulePtr GetIthRule(int i) const {
    TRulePtr nr(new TRule(*r));
    nr->e_ = cur_trg_chunks[i];
    //cerr << nr->AsString() << endl;
    return nr;
  }
  virtual int Arity() const {
    return 0;
  }
  virtual const RuleBin* GetRules() const {
    if (!r) return NULL; else return this;
  }
  virtual const GrammarIter* Extend(int symbol) const {
    if (symbol <= 0) return NULL;
    if (!r || !kMAX_SRC_SIZE || r->f_.size() < kMAX_SRC_SIZE)
      return new TransliterationGrammarIter(r, symbol);
    else
      return NULL;
  }
  TRulePtr r;
};

struct TransliterationGrammar : public Grammar {
  virtual const GrammarIter* GetRoot() const {
    return new TransliterationGrammarIter;
  }
  virtual bool HasRuleForSpan(int, int, int distance) const {
    return (distance < kMAX_SRC_SIZE);
  }
};

struct TInfo {
  TInfo() : initialized(false) {}
  bool initialized;
  Hypergraph lattice;   // may be empty if transliteration is not possible
  prob_t est_prob;      // will be zero if not possible
};

struct TransliterationsImpl {
  TransliterationsImpl() {
    kX = TD::Convert("X")*-1;
    kMAX_SRC_SIZE = 4;
    grammars.push_back(GrammarPtr(new TransliterationGrammar));
    grammars.push_back(GrammarPtr(new GlueGrammar("S", "X")));
    SetSilent(true);
  }

  void Initialize(WordID src, const vector<WordID>& src_lets, WordID trg, const vector<WordID>& trg_lets) {
    if (src >= graphs.size()) graphs.resize(src + 1);
    if (graphs[src][trg].initialized) return;
    int kMAX_TRG_SIZE = 4;
    InitTargetChunks(kMAX_TRG_SIZE, trg_lets);
    ExhaustiveBottomUpParser parser("S", grammars);
    Lattice lat(src_lets.size()), tlat(trg_lets.size());
    for (unsigned i = 0; i < src_lets.size(); ++i)
      lat[i].push_back(LatticeArc(src_lets[i], 0.0, 1));
    for (unsigned i = 0; i < trg_lets.size(); ++i)
      tlat[i].push_back(LatticeArc(trg_lets[i], 0.0, 1));
    //cerr << "Creating lattice for: " << TD::Convert(src) << " --> " << TD::Convert(trg) << endl;
    //cerr << "'" << TD::GetString(src_lets) << "' --> " << TD::GetString(trg_lets) << endl;
    if (!parser.Parse(lat, &graphs[src][trg].lattice)) {
      //cerr << "Failed to parse " << TD::GetString(src_lets) << endl;
      abort();
    }
    if (HG::Intersect(tlat, &graphs[src][trg].lattice)) {
      graphs[src][trg].est_prob = prob_t(1e-4);
    } else {
      graphs[src][trg].lattice.clear();
      //cerr << "Failed to intersect " << TD::GetString(src_lets) << " ||| " << TD::GetString(trg_lets) << endl;
      graphs[src][trg].est_prob = prob_t::Zero();
    }
    for (unsigned i = 0; i < tlttofreelist.size(); ++i)
      delete tlttofreelist[i];
    tlttofreelist.clear();
    //cerr << "Number of paths: " << graphs[src][trg].lattice.NumberOfPaths() << endl;
    graphs[src][trg].initialized = true;
  }

  const prob_t& EstimateProbability(WordID src, WordID trg) const {
    assert(src < graphs.size());
    const unordered_map<WordID, TInfo>& um = graphs[src];
    const unordered_map<WordID, TInfo>::const_iterator it = um.find(trg);
    assert(it != um.end());
    assert(it->second.initialized);
    return it->second.est_prob;
  }

  void Forbid(WordID src, WordID trg) {
    if (src >= graphs.size()) graphs.resize(src + 1);
    graphs[src][trg].est_prob = prob_t::Zero();
    graphs[src][trg].initialized = true;
  }

  void GraphSummary() const {
    double tlp = 0;
    int tt = 0;
    for (int i = 0; i < graphs.size(); ++i) {
      const unordered_map<WordID, TInfo>& um = graphs[i];
      unordered_map<WordID, TInfo>::const_iterator it;
      for (it = um.begin(); it != um.end(); ++it) {
        if (it->second.lattice.empty()) continue;
        //cerr << TD::Convert(i) << " --> " << TD::Convert(it->first) << ": " << it->second.lattice.NumberOfPaths() << endl;
        tlp += log(it->second.lattice.NumberOfPaths());
        tt++;
      }
    }
    tlp /= tt;
    cerr << "E[log paths] = " << tlp << endl;
    cerr << "exp(E[log paths]) = " << exp(tlp) << endl;
  }

  vector<unordered_map<WordID, TInfo> > graphs;
  vector<GrammarPtr> grammars;
};

Transliterations::Transliterations() : pimpl_(new TransliterationsImpl) {}
Transliterations::~Transliterations() { delete pimpl_; }

void Transliterations::Initialize(WordID src, const vector<WordID>& src_lets, WordID trg, const vector<WordID>& trg_lets) {
  pimpl_->Initialize(src, src_lets, trg, trg_lets);
}

prob_t Transliterations::EstimateProbability(WordID src, WordID trg) const {
  return pimpl_->EstimateProbability(src,trg);
}

void Transliterations::Forbid(WordID src, WordID trg) {
  pimpl_->Forbid(src, trg);
}

void Transliterations::GraphSummary() const {
  pimpl_->GraphSummary();
}


