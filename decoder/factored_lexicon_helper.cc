#include "factored_lexicon_helper.h"

#include "filelib.h"
#include "stringlib.h"
#include "sentence_metadata.h"

using namespace std;

FactoredLexiconHelper::FactoredLexiconHelper() :
    kNULL(TD::Convert("<eps>")),
    has_src_(false),
    has_trg_(false) { InitEscape(); }

FactoredLexiconHelper::FactoredLexiconHelper(const std::string& srcfile, const std::string& trgmapfile) :
    kNULL(TD::Convert("<eps>")),
    has_src_(false),
    has_trg_(false) {
  if (srcfile.size() && srcfile != "*") {
    ReadFile rf(srcfile);
    has_src_ = true;
    istream& in = *rf.stream();
    string line;
    while(in) {
      getline(in, line);
      if (!in) continue;
      vector<WordID> v;
      TD::ConvertSentence(line, &v);
      src_.push_back(v);
    }
  }
  if (trgmapfile.size() && trgmapfile != "*") {
    ReadFile rf(trgmapfile);
    has_trg_ = true;
    istream& in = *rf.stream();
    string line;
    vector<string> v;
    while(in) {
      getline(in, line);
      if (!in) continue;
      SplitOnWhitespace(line, &v);
      if (v.size() != 2) {
        cerr << "Error reading line in map file: " << line << endl;
        abort();
      }
      WordID& to = trgmap_[TD::Convert(v[0])];
      if (to != 0) {
        cerr << "Duplicate entry for word " << v[0] << endl;
        abort();
      }
      to = TD::Convert(v[1]);
    }
  }
  InitEscape();
}

void FactoredLexiconHelper::InitEscape() {
  escape_[TD::Convert("=")] = TD::Convert("__EQ");
  escape_[TD::Convert(";")] = TD::Convert("__SC");
  escape_[TD::Convert(",")] = TD::Convert("__CO");
}

void FactoredLexiconHelper::PrepareForInput(const SentenceMetadata& smeta) {
  if (has_src_) {
    const int id = smeta.GetSentenceID();
    assert(id < src_.size());
    cur_src_ = src_[id];
  } else {
    cur_src_.resize(smeta.GetSourceLength());
    for (int i = 0; i < cur_src_.size(); ++i) {
      const vector<LatticeArc>& arcs = smeta.GetSourceLattice()[i];
      assert(arcs.size() == 1);    // only sentences supported for now
      cur_src_[i] = arcs[0].label;
    }
  }
  if (cur_src_.size() != smeta.GetSourceLength()) {
    cerr << "Length mismatch between mapped source and real source in sentence id=" << smeta.GetSentenceID() << endl;
    cerr << "  mapped len=" << cur_src_.size() << endl;
    cerr << "  actual len=" << smeta.GetSourceLength() << endl;
  }
}

