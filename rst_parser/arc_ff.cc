#include "arc_ff.h"

#include "tdict.h"
#include "fdict.h"
#include "sentence_metadata.h"

using namespace std;

ArcFeatureFunction::~ArcFeatureFunction() {}

void ArcFeatureFunction::PrepareForInput(const TaggedSentence&) {}

DistancePenalty::DistancePenalty(const string&) : fidw_(FD::Convert("Distance")), fidr_(FD::Convert("RootDistance")) {}

void DistancePenalty::EdgeFeaturesImpl(const TaggedSentence& sent,
                                       short h,
                                       short m,
                                       SparseVector<weight_t>* features) const {
  const bool dir = m < h;
  const bool is_root = (h == -1);
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
    os << "ROOT:" << TD::Convert(sent.pos[m]);
    features->set_value(FD::Convert(os.str()), 1.0);
    os << "_" << lenstr;
    features->set_value(FD::Convert(os.str()), 1.0);
  } else { // not root
    ostringstream os;
    os << "HM:" << TD::Convert(sent.pos[h]) << '_' << TD::Convert(sent.pos[m]);
    features->set_value(FD::Convert(os.str()), 1.0);
    os << '_' << dir;
    features->set_value(FD::Convert(os.str()), 1.0);
    os << '_' << lenstr;
    features->set_value(FD::Convert(os.str()), 1.0);
    ostringstream os2;
    os2 << "LexHM:" << TD::Convert(sent.words[h]) << '_' << TD::Convert(sent.words[m]);
    features->set_value(FD::Convert(os2.str()), 1.0);
    os2 << '_' << dir;
    features->set_value(FD::Convert(os2.str()), 1.0);
    os2 << '_' << lenstr;
    features->set_value(FD::Convert(os2.str()), 1.0);
  }
}
