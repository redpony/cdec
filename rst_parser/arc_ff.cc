#include "arc_ff.h"

#include "tdict.h"
#include "fdict.h"
#include "sentence_metadata.h"

using namespace std;

struct ArcFFImpl {
  ArcFFImpl() : kROOT("ROOT") {}
  const string kROOT;

  void PrepareForInput(const TaggedSentence& sentence) {
    (void) sentence;
  }

  void EdgeFeatures(const TaggedSentence& sent,
                    short h,
                    short m,
                    SparseVector<weight_t>* features) const {
    const bool is_root = (h == -1);
    const string& head_word = (is_root ? kROOT : TD::Convert(sent.words[h]));
    const string& head_pos = (is_root ? kROOT : TD::Convert(sent.pos[h]));
    const string& mod_word = TD::Convert(sent.words[m]);
    const string& mod_pos = TD::Convert(sent.pos[m]);
    const bool dir = m < h;
    int v = m - h;
    if (v < 0) {
      v= -1 - int(log(-v) / log(2));
    } else {
      v= int(log(v) / log(2));
    }
    static map<int, int> lenmap;
    int& lenfid = lenmap[v];
    if (!lenfid) {
      ostringstream os;
      if (v < 0) os << "LenL" << -v; else os << "LenR" << v;
      lenfid = FD::Convert(os.str());
    }
    features->set_value(lenfid, 1.0);
    const string& lenstr = FD::Convert(lenfid);
    if (!is_root) {
      static int modl = FD::Convert("ModLeft");
      static int modr = FD::Convert("ModRight");
      if (dir) features->set_value(modl, 1);
      else features->set_value(modr, 1);
    }
    if (is_root) {
      ostringstream os;
      os << "ROOT:" << mod_pos;
      features->set_value(FD::Convert(os.str()), 1.0);
      os << "_" << lenstr;
      features->set_value(FD::Convert(os.str()), 1.0);
    } else { // not root
      ostringstream os;
      os << "HM:" << head_pos << '_' << mod_pos;
      features->set_value(FD::Convert(os.str()), 1.0);
      os << '_' << dir;
      features->set_value(FD::Convert(os.str()), 1.0);
      os << '_' << lenstr;
      features->set_value(FD::Convert(os.str()), 1.0);
      ostringstream os2;
      os2 << "LexHM:" << head_word << '_' << mod_word;
      features->set_value(FD::Convert(os2.str()), 1.0);
      os2 << '_' << dir;
      features->set_value(FD::Convert(os2.str()), 1.0);
      os2 << '_' << lenstr;
      features->set_value(FD::Convert(os2.str()), 1.0);
    }
  }
};

ArcFeatureFunctions::ArcFeatureFunctions() : pimpl(new ArcFFImpl) {}
ArcFeatureFunctions::~ArcFeatureFunctions() { delete pimpl; }

void ArcFeatureFunctions::PrepareForInput(const TaggedSentence& sentence) {
  pimpl->PrepareForInput(sentence);
}

void ArcFeatureFunctions::EdgeFeatures(const TaggedSentence& sentence,
                                       short h,
                                       short m,
                                       SparseVector<weight_t>* features) const {
  pimpl->EdgeFeatures(sentence, h, m, features);
}

