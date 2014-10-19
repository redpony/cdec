#ifndef TRULE_H_
#define TRULE_H_

#include <algorithm>
#include <vector>
#include <cassert>
#include <iostream>

#include "boost/shared_ptr.hpp"
#include "boost/functional/hash.hpp"

#include "sparse_vector.h"
#include "wordid.h"
#include "tdict.h"

class TRule;
typedef boost::shared_ptr<TRule> TRulePtr;

namespace cdec { struct TreeFragment; }

struct AlignmentPoint {
  AlignmentPoint() : s_(), t_() {}
  AlignmentPoint(int s, int t) : s_(s), t_(t) {}
  AlignmentPoint Inverted() const {
    return AlignmentPoint(t_, s_);
  }
  short s_;
  short t_;
};

inline std::ostream& operator<<(std::ostream& os, const AlignmentPoint& p) {
  return os << static_cast<int>(p.s_) << '-' << static_cast<int>(p.t_);
}


// Translation rule
class TRule {
 public:
  TRule() : lhs_(0), prev_i(-1), prev_j(-1) { }
  TRule(WordID lhs, const WordID* src, int src_size, const WordID* trg, int trg_size, const int* feat_ids, const double* feat_vals, int feat_size, int arity, const AlignmentPoint* als, int alsnum) :
      e_(trg, trg + trg_size), f_(src, src + src_size), lhs_(lhs), arity_(arity), prev_i(-1), prev_j(-1),
      a_(als, als + alsnum) {
    for (int i = 0; i < feat_size; ++i)
      scores_.set_value(feat_ids[i], feat_vals[i]);
  }

  TRule(WordID lhs, const WordID* src, int src_size, const WordID* trg, int trg_size, int arity, int pi, int pj) :
    e_(trg, trg + trg_size), f_(src, src + src_size), lhs_(lhs), arity_(arity), prev_i(pi), prev_j(pj) {}

  bool IsGoal() const;

  explicit TRule(const std::vector<WordID>& e) : e_(e), lhs_(0), prev_i(-1), prev_j(-1) {}
  TRule(const std::vector<WordID>& e, const std::vector<WordID>& f, const WordID& lhs) :
    e_(e), f_(f), lhs_(lhs), prev_i(-1), prev_j(-1) {}

  TRule(const TRule& other) :
    e_(other.e_), f_(other.f_), lhs_(other.lhs_), scores_(other.scores_), arity_(other.arity_), prev_i(-1), prev_j(-1), a_(other.a_) {}

  explicit TRule(const std::string& text, bool mono = false) : prev_i(-1), prev_j(-1) {
    ReadFromString(text, mono);
  }

  // make a rule from a hiero-like rule table, e.g.
  //    [X] ||| [X,1] DE [X,2] ||| [X,2] of the [X,1]
  static TRule* CreateRuleSynchronous(const std::string& rule);

  // make a rule from a phrasetable entry (i.e., one that has no LHS type), e.g:
  //    el gato ||| the cat ||| Feature_2=0.34
  static TRule* CreateRulePhrasetable(const std::string& rule);

  // make a rule from a non-synchrnous CFG representation, e.g.:
  //    [LHS] ||| term1 [NT] term2 [OTHER_NT] [YET_ANOTHER_NT]
  static TRule* CreateRuleMonolingual(const std::string& rule);

  static TRule* CreateLexicalRule(const WordID& src, const WordID& trg) {
    return new TRule(src, trg);
  }

  void ESubstitute(const std::vector<const std::vector<WordID>* >& var_values,
                   std::vector<WordID>* result) const {
    unsigned vc = 0;
    result->clear();
    for (const auto& c : e_) {
      if (c < 1) {
        ++vc;
        const auto& var_value = *var_values[-c];
        std::copy(var_value.begin(),
                  var_value.end(),
                  std::back_inserter(*result));
      } else {
        result->push_back(c);
      }
    }
    assert(vc == var_values.size());
  }

  void FSubstitute(const std::vector<const std::vector<WordID>* >& var_values,
                   std::vector<WordID>* result) const {
    unsigned vc = 0;
    result->clear();
    for (const auto& c : f_) {
      if (c < 1) {
        const auto& var_value = *var_values[vc++];
        std::copy(var_value.begin(),
                  var_value.end(),
                  std::back_inserter(*result));
      } else {
        result->push_back(c);
      }
    }
    assert(vc == var_values.size());
  }

  bool ReadFromString(const std::string& line, bool monolingual = false);

  bool Initialized() const { return e_.size(); }

  std::string AsString(bool verbose = true) const;
  friend std::ostream &operator<<(std::ostream &o,TRule const& r);
  static TRule DummyRule() {
    TRule res;
    res.e_.resize(1, 0);
    return res;
  }

  const std::vector<WordID>& f() const { return f_; }
  const std::vector<WordID>& e() const { return e_; }
  const std::vector<AlignmentPoint>& als() const { return a_; }

  int EWords() const { return ELength() - Arity(); }
  int FWords() const { return FLength() - Arity(); }
  int FLength() const { return f_.size(); }
  int ELength() const { return e_.size(); }
  int Arity() const { return arity_; }
  bool IsUnary() const { return (Arity() == 1) && (f_.size() == 1); }
  const SparseVector<double>& GetFeatureValues() const { return scores_; }
  double Score(int i) const { return scores_.value(i); }
  WordID GetLHS() const { return lhs_; }
  void ComputeArity();

  // 0 = first variable, -1 = second variable, -2 = third ..., i.e. tail_nodes_[-w] if w<=0, TD::Convert(w) otherwise
  std::vector<WordID> e_;
  // < 0: *-1 = encoding of category of variable
  std::vector<WordID> f_;
  WordID lhs_;
  SparseVector<double> scores_;

  char arity_;
  std::vector<WordID> ext_states_; // in t2s or t2t translation, this is of length arity_ and
                                   // indicates what state the transducer is in after having processed
                                   // this transduction rule

  // these attributes are application-specific and should probably be refactored
  TRulePtr parent_rule_;  // usually NULL, except when doing constrained decoding

  // this is only used when doing synchronous parsing
  short int prev_i;
  short int prev_j;

  std::vector<AlignmentPoint> a_;  // alignment points, may be empty

  // only for coarse-to-fine decoding
  boost::shared_ptr<std::vector<TRulePtr> > fine_rules_;

  // optional, shows internal structure of TSG rules
  boost::shared_ptr<cdec::TreeFragment> tree_structure;

  friend class boost::serialization::access;
  template<class Archive>
  void save(Archive & ar, const unsigned int /*version*/) const {
    ar & TD::Convert(-lhs_);
    unsigned f_size = f_.size();
    ar & f_size;
    assert(f_size <= (sizeof(size_t) * 8));
    size_t f_nt_mask = 0;
    for (int i = f_.size() - 1; i >= 0; --i) {
      f_nt_mask <<= 1;
      f_nt_mask |= (f_[i] <= 0 ? 1 : 0);
    }
    ar & f_nt_mask;
    for (unsigned i = 0; i < f_.size(); ++i)
      ar & TD::Convert(f_[i] < 0 ? -f_[i] : f_[i]);
    unsigned e_size = e_.size();
    ar & e_size;
    size_t e_nt_mask = 0;
    assert(e_size <= (sizeof(size_t) * 8));
    for (int i = e_.size() - 1; i >= 0; --i) {
      e_nt_mask <<= 1;
      e_nt_mask |= (e_[i] <= 0 ? 1 : 0);
    }
    ar & e_nt_mask;
    for (unsigned i = 0; i < e_.size(); ++i)
      if (e_[i] <= 0) ar & e_[i]; else ar & TD::Convert(e_[i]);
    ar & arity_;
    ar & scores_;
  }
  template<class Archive>
  void load(Archive & ar, const unsigned int /*version*/) {
    std::string lhs; ar & lhs; lhs_ = -TD::Convert(lhs);
    unsigned f_size; ar & f_size;
    f_.resize(f_size);
    size_t f_nt_mask; ar & f_nt_mask;
    std::string sym;
    for (unsigned i = 0; i < f_size; ++i) {
      bool mask = (f_nt_mask & 1);
      ar & sym;
      f_[i] = TD::Convert(sym) * (mask ? -1 : 1);
      f_nt_mask >>= 1;
    }
    unsigned e_size; ar & e_size;
    e_.resize(e_size);
    size_t e_nt_mask; ar & e_nt_mask;
    for (unsigned i = 0; i < e_size; ++i) {
      bool mask = (e_nt_mask & 1);
      if (mask) {
        ar & e_[i];
      } else {
        ar & sym;
        e_[i] = TD::Convert(sym);
      }
      e_nt_mask >>= 1;
    }
    ar & arity_;
    ar & scores_;
  }

  BOOST_SERIALIZATION_SPLIT_MEMBER()
 private:
  TRule(const WordID& src, const WordID& trg) : e_(1, trg), f_(1, src), lhs_(), arity_(), prev_i(), prev_j() {}
};

inline size_t hash_value(const TRule& r) {
  size_t h = boost::hash_value(r.e_);
  boost::hash_combine(h, -r.lhs_);
  boost::hash_combine(h, boost::hash_value(r.f_));
  return h;
}

inline bool operator==(const TRule& a, const TRule& b) {
  return (a.lhs_ == b.lhs_ && a.e_ == b.e_ && a.f_ == b.f_);
}

#endif
