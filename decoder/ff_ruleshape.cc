#include "ff_ruleshape.h"

#include "filelib.h"
#include "stringlib.h"
#include "verbose.h"
#include "trule.h"
#include "hg.h"
#include "fdict.h"
#include <sstream>

using namespace std;

inline bool IsBitSet(int i, int bit) {
  const int mask = 1 << bit;
  return (i & mask);
}

inline char BitAsChar(bool bit) {
  return (bit ? '1' : '0');
}

RuleShapeFeatures::RuleShapeFeatures(const string& /* param */) {
  bool first = true;
  for (int i = 0; i < 32; ++i) {
    for (int j = 0; j < 32; ++j) {
      ostringstream os;
      os << "Shape_S";
      Node* cur = &fidtree_;
      for (int k = 0; k < 5; ++k) {
        bool bit = IsBitSet(i,k);
        cur = &cur->next_[bit];
        os << BitAsChar(bit);
      }
      os << "_T";
      for (int k = 0; k < 5; ++k) {
        bool bit = IsBitSet(j,k);
        cur = &cur->next_[bit];
        os << BitAsChar(bit);
      }
      if (first) { first = false; cerr << "  Example feature: " << os.str() << endl; }
      cur->fid_ = FD::Convert(os.str());
    }
  }
}

void RuleShapeFeatures::TraversalFeaturesImpl(const SentenceMetadata& /* smeta */,
                                              const Hypergraph::Edge& edge,
                                              const vector<const void*>& /* ant_contexts */,
                                              SparseVector<double>* features,
                                              SparseVector<double>* /* estimated_features */,
                                              void* /* context */) const {
  const Node* cur = &fidtree_;
  TRule& rule = *edge.rule_;
  int pos = 0;  // feature position
  int i = 0;
  while(i < rule.f_.size()) {
    WordID sym = rule.f_[i];
    if (pos % 2 == 0) {
      if (sym > 0) {       // is terminal
        cur = Advance(cur, true);
        while (i < rule.f_.size() && rule.f_[i] > 0) ++i;  // consume lexical string
      } else {
        cur = Advance(cur, false);
      }
      ++pos;
    } else {  // expecting a NT
      if (sym < 1) {
        cur = Advance(cur, true);
        ++i;
        ++pos;
      } else {
        cerr << "BAD RULE: " << rule.AsString() << endl;
        exit(1);
      }
    }
  }
  for (; pos < 5; ++pos)
    cur = Advance(cur, false);
  assert(pos == 5);  // this will fail if you are using using > binary rules!

  i = 0;
  while(i < rule.e_.size()) {
    WordID sym = rule.e_[i];
    if (pos % 2 == 1) {
      if (sym > 0) {       // is terminal
        cur = Advance(cur, true);
        while (i < rule.e_.size() && rule.e_[i] > 0) ++i;  // consume lexical string
      } else {
        cur = Advance(cur, false);
      }
      ++pos;
    } else {  // expecting a NT
      if (sym < 1) {
        cur = Advance(cur, true);
        ++i;
        ++pos;
      } else {
        cerr << "BAD RULE: " << rule.AsString() << endl;
        exit(1);
      }
    }
  }
  for (;pos < 10; ++pos)
    cur = Advance(cur, false);
  assert(pos == 10);  // this will fail if you are using using > binary rules!

  features->set_value(cur->fid_, 1.0);
}

namespace {
void ParseRSArgs(string const& in, string* emapfile, string* fmapfile, unsigned *pfxsize) {
  vector<string> const& argv=SplitOnWhitespace(in);
  *emapfile = "";
  *fmapfile = "";
  *pfxsize = 0;
#define RSSPEC_NEXTARG if (i==argv.end()) {            \
    cerr << "Missing argument for "<<*last<<". "; goto usage; \
    } else { ++i; }

  for (vector<string>::const_iterator last,i=argv.begin(),e=argv.end();i!=e;++i) {
    string const& s=*i;
    if (s[0]=='-') {
      if (s.size()>2) goto fail;
      switch (s[1]) {
      case 'e':
        if (emapfile->size() > 0) { cerr << "Multiple -e specifications!\n"; abort(); }
        RSSPEC_NEXTARG; *emapfile=*i;
        break;
      case 'f':
        if (fmapfile->size() > 0) { cerr << "Multiple -f specifications!\n"; abort(); }
        RSSPEC_NEXTARG; *fmapfile=*i;
        break;
      case 'p':
        RSSPEC_NEXTARG; *pfxsize=atoi(i->c_str());
        break;
#undef RSSPEC_NEXTARG
      default:
      fail:
        cerr<<"Unknown RuleShape2 option "<<s<<" ; ";
        goto usage;
      }
    } else {
      cerr << "RuleShape2 bad argument!\n";
      abort();
    }
  }
  return;
usage:
  cerr << "Bad parameters for RuleShape2\n";
  abort();
}

inline void AddWordToClassMapping_(vector<WordID>* pv, unsigned f, unsigned t, unsigned pfx_size) {
  if (pfx_size) {
    const string& ts = TD::Convert(t);
    if (pfx_size < ts.size())
      t = TD::Convert(ts.substr(0, pfx_size));
  }
  if (f >= pv->size())
    pv->resize((f + 1) * 1.2);
  (*pv)[f] = t;
}
}

RuleShapeFeatures2::~RuleShapeFeatures2() {}

RuleShapeFeatures2::RuleShapeFeatures2(const string& param) : kNT(TD::Convert("NT")), kUNK(TD::Convert("<unk>")) {
  string emap;
  string fmap;
  unsigned pfxsize = 0;
  ParseRSArgs(param, &emap, &fmap, &pfxsize);
  has_src_ = fmap.size();
  has_trg_ = emap.size();
  if (has_trg_) LoadWordClasses(emap, pfxsize, &e2class_);
  if (has_src_) LoadWordClasses(fmap, pfxsize, &f2class_);
  if (!has_trg_ && !has_src_) {
    cerr << "RuleShapeFeatures2 requires [-e trg_map.gz] or [-f src_map.gz] or both, and optional [-p pfxsize]\n";
    abort();
  }
}

void RuleShapeFeatures2::LoadWordClasses(const string& file, const unsigned pfx_size, vector<WordID>* pv) {
  ReadFile rf(file);
  istream& in = *rf.stream();
  string line;
  vector<WordID> dummy;
  int lc = 0;
  if (!SILENT)
    cerr << "  Loading word classes from " << file << " ...\n";
  AddWordToClassMapping_(pv, TD::Convert("<s>"), TD::Convert("<s>"), 0);
  AddWordToClassMapping_(pv, TD::Convert("</s>"), TD::Convert("</s>"), 0);
  while(getline(in, line)) {
    dummy.clear();
    TD::ConvertSentence(line, &dummy);
    ++lc;
    if (dummy.size() != 2 && dummy.size() != 3) {
      cerr << "    Class map file expects: CLASS WORD [freq]\n";
      cerr << "    Format error in " << file << ", line " << lc << ": " << line << endl;
      abort();
    }
    AddWordToClassMapping_(pv, dummy[1], dummy[0], pfx_size);
  }
  if (!SILENT)
    cerr << "  Loaded word " << lc << " mapping rules.\n";
}

void RuleShapeFeatures2::TraversalFeaturesImpl(const SentenceMetadata& /* smeta */,
                                               const Hypergraph::Edge& edge,
                                               const vector<const void*>& /* ant_contexts */,
                                               SparseVector<double>* features,
                                               SparseVector<double>* /* estimated_features */,
                                               void* /* context */) const {
  const vector<int>& f = edge.rule_->f();
  const vector<int>& e = edge.rule_->e();
  Node* fid = &fidtree_;
  if (has_src_) {
    for (unsigned i = 0; i < f.size(); ++i)
      fid = &fid->next_[MapF(f[i])];
  }
  if (has_trg_) {
    for (unsigned i = 0; i < e.size(); ++i)
      fid = &fid->next_[MapE(e[i])];
  }
  if (!fid->fid_) {
    ostringstream os;
    os << "RS:";
    if (has_src_) {
      for (unsigned i = 0; i < f.size(); ++i) {
        if (i) os << '_';
        os << TD::Convert(MapF(f[i]));
      }
      if (has_trg_) os << "__";
    }
    if (has_trg_) {
      for (unsigned i = 0; i < e.size(); ++i) {
        if (i) os << '_';
        os << TD::Convert(MapE(e[i]));
      }
    }
    fid->fid_ = FD::Convert(os.str());
  }
  features->set_value(fid->fid_, 1);
}

