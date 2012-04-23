#include "arc_ff.h"

#include <iostream>
#include <sstream>

#include "stringlib.h"
#include "tdict.h"
#include "fdict.h"
#include "sentence_metadata.h"

using namespace std;

struct ArcFFImpl {
  ArcFFImpl() : kROOT("ROOT"), kLEFT_POS("LEFT"), kRIGHT_POS("RIGHT") {}
  const string kROOT;
  const string kLEFT_POS;
  const string kRIGHT_POS;
  map<WordID, vector<int> > pcs;

  void PrepareForInput(const TaggedSentence& sent) {
    pcs.clear();
    for (int i = 0; i < sent.pos.size(); ++i)
      pcs[sent.pos[i]].resize(1, 0);
    pcs[sent.pos[0]][0] = 1;
    for (int i = 1; i < sent.pos.size(); ++i) {
      const WordID posi = sent.pos[i];
      for (map<WordID, vector<int> >::iterator j = pcs.begin(); j != pcs.end(); ++j) {
        const WordID posj = j->first;
        vector<int>& cs = j->second;
        cs.push_back(cs.back() + (posj == posi ? 1 : 0));
      }
    }
  }

  template <typename A>
  static void Fire(SparseVector<weight_t>* v, const A& a) {
    ostringstream os;
    os << a;
    v->set_value(FD::Convert(os.str()), 1);
  }

  template <typename A, typename B>
  static void Fire(SparseVector<weight_t>* v, const A& a, const B& b) {
    ostringstream os;
    os << a << ':' << b;
    v->set_value(FD::Convert(os.str()), 1);
  }

  template <typename A, typename B, typename C>
  static void Fire(SparseVector<weight_t>* v, const A& a, const B& b, const C& c) {
    ostringstream os;
    os << a << ':' << b << '_' << c;
    v->set_value(FD::Convert(os.str()), 1);
  }

  template <typename A, typename B, typename C, typename D>
  static void Fire(SparseVector<weight_t>* v, const A& a, const B& b, const C& c, const D& d) {
    ostringstream os;
    os << a << ':' << b << '_' << c << '_' << d;
    v->set_value(FD::Convert(os.str()), 1);
  }

  template <typename A, typename B, typename C, typename D, typename E>
  static void Fire(SparseVector<weight_t>* v, const A& a, const B& b, const C& c, const D& d, const E& e) {
    ostringstream os;
    os << a << ':' << b << '_' << c << '_' << d << '_' << e;
    v->set_value(FD::Convert(os.str()), 1);
  }

  static void AddConjoin(const SparseVector<double>& v, const string& feat, SparseVector<double>* pf) {
    for (SparseVector<double>::const_iterator it = v.begin(); it != v.end(); ++it)
      pf->set_value(FD::Convert(FD::Convert(it->first) + "_" + feat), it->second);
  }

  static inline string Fixup(const string& str) {
    string res = LowercaseString(str);
    if (res.size() < 6) return res;
    return res.substr(0, 5) + "*";
  }

  static inline string Suffix(const string& str) {
    if (str.size() < 4) return ""; else return str.substr(str.size() - 3);
  }

  void EdgeFeatures(const TaggedSentence& sent,
                    short h,
                    short m,
                    SparseVector<weight_t>* features) const {
    const bool is_root = (h == -1);
    const string head_word = (is_root ? kROOT : Fixup(TD::Convert(sent.words[h])));
    int num_words = sent.words.size();
    const string& head_pos = (is_root ? kROOT : TD::Convert(sent.pos[h]));
    const string mod_word = Fixup(TD::Convert(sent.words[m]));
    const string& mod_pos = TD::Convert(sent.pos[m]);
    const string& mod_pos_L = (m > 0 ? TD::Convert(sent.pos[m-1]) : kLEFT_POS);
    const string& mod_pos_R = (m < sent.pos.size() - 1 ? TD::Convert(sent.pos[m]) : kRIGHT_POS);
    const bool bdir = m < h;
    const string dir = (bdir ? "MLeft" : "MRight");
    int v = m - h;
    if (v < 0) {
      v= -1 - int(log(-v) / log(1.6));
    } else {
      v= int(log(v) / log(1.6)) + 1;
    }
    ostringstream os;
    if (v < 0) os << "LenL" << -v; else os << "LenR" << v;
    const string lenstr = os.str();
    Fire(features, dir);
    Fire(features, lenstr);
    // dir, lenstr
    if (is_root) {
      Fire(features, "wROOT", mod_word);
      Fire(features, "pROOT", mod_pos);
      Fire(features, "wpROOT", mod_word, mod_pos);
      Fire(features, "DROOT", mod_pos, lenstr);
      Fire(features, "LROOT", mod_pos_L);
      Fire(features, "RROOT", mod_pos_R);
      Fire(features, "LROOT", mod_pos_L, mod_pos);
      Fire(features, "RROOT", mod_pos_R, mod_pos);
      Fire(features, "LDist", m);
      Fire(features, "RDist", num_words - m);
    } else { // not root
      const string& head_pos_L = (h > 0 ? TD::Convert(sent.pos[h-1]) : kLEFT_POS);
      const string& head_pos_R = (h < sent.pos.size() - 1 ? TD::Convert(sent.pos[h]) : kRIGHT_POS);
      SparseVector<double> fv;
      SparseVector<double>* f = &fv;
      Fire(f, "H", head_pos);
      Fire(f, "M", mod_pos);
      Fire(f, "HM", head_pos, mod_pos);

      // surrounders
      Fire(f, "posLL", head_pos, mod_pos, head_pos_L, mod_pos_L);
      Fire(f, "posRR", head_pos, mod_pos, head_pos_R, mod_pos_R);
      Fire(f, "posLR", head_pos, mod_pos, head_pos_L, mod_pos_R);
      Fire(f, "posRL", head_pos, mod_pos, head_pos_R, mod_pos_L);

      // between features
      int left = min(h,m);
      int right = max(h,m);
      if (right - left >= 2) {
        if (bdir) --right; else ++left;
        for (map<WordID, vector<int> >::const_iterator it = pcs.begin(); it != pcs.end(); ++it) {
          if (it->second[left] != it->second[right]) {
            Fire(f, "BT", head_pos, TD::Convert(it->first), mod_pos);
          }
        }
      }

      Fire(f, "wH", head_word);
      Fire(f, "wM", mod_word);
      Fire(f, "wpH", head_word, head_pos);
      Fire(f, "wpM", mod_word, mod_pos);
      Fire(f, "pHwM", head_pos, mod_word);
      Fire(f, "wHpM", head_word, mod_pos);

      Fire(f, "wHM", head_word, mod_word);
      Fire(f, "pHMwH", head_pos, mod_pos, head_word);
      Fire(f, "pHMwM", head_pos, mod_pos, mod_word);
      Fire(f, "wHMpH", head_word, mod_word, head_pos);
      Fire(f, "wHMpM", head_word, mod_word, mod_pos);
      Fire(f, "wHMpHM", head_word, mod_word, head_pos, mod_pos);

      AddConjoin(fv, dir, features);
      AddConjoin(fv, lenstr, features);
      (*features) += fv;
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

