#include "ff_const_reorder.h"

#include "filelib.h"
#include "stringlib.h"
#include "hg.h"
#include "sentence_metadata.h"
#include "hash.h"
#include "ff_const_reorder_common.h"

#include <sstream>
#include <string>
#include <vector>
#include <stdio.h>

using namespace std;
using namespace const_reorder;

typedef HASH_MAP<std::string, vector<double> > MapClassifier;

inline bool is_inside(int i, int left, int right) {
  if (i < left || i > right) return false;
  return true;
}

/*
 * assume i <= j
 * [i, j] is inside [left, right] or [i, j] equates to [left, right]
 */
inline bool is_inside(int i, int j, int left, int right) {
  if (i >= left && j <= right) return true;
  return false;
}

/*
 * assume i <= j
 * [i, j] is inside [left, right], but [i, j] not equal to [left, right]
 */
inline bool is_proper_inside(int i, int j, int left, int right) {
  if (i >= left && j <= right && right - left > j - i) return true;
  return false;
}

/*
 * assume i <= j
 * [i, j] is proper proper inside [left, right]
 */
inline bool is_proper_proper_inside(int i, int j, int left, int right) {
  if (i > left && j < right) return true;
  return false;
}

inline bool is_overlap(int left1, int right1, int left2, int right2) {
  if (is_inside(left1, left2, right2) || is_inside(left2, left1, right1))
    return true;

  return false;
}

inline void NewAndCopyCharArray(char** p, const char* q) {
  if (q != NULL) {
    (*p) = new char[strlen(q) + 1];
    strcpy((*p), q);
  } else
    (*p) = NULL;
}

// TODO:to make the alignment more efficient
struct TargetTranslation {
  TargetTranslation(int begin_pos, int end_pos,int e_num_word)
      : begin_pos_(begin_pos),
        end_pos_(end_pos),
        e_num_words_(e_num_word),
        vec_left_most_(end_pos - begin_pos + 1, e_num_word),
        vec_right_most_(end_pos - begin_pos + 1, -1),
        vec_f_align_bit_array_(end_pos - begin_pos + 1),
        vec_e_align_bit_array_(e_num_word) {
    int len = end_pos - begin_pos + 1;
    align_.reserve(1.5 * len);
  }

  void InsertAlignmentPoint(int s, int t) {
    int i = s - begin_pos_;

    vector<bool>& b = vec_f_align_bit_array_[i];
    if (b.empty()) b.resize(e_num_words_);
    b[t] = 1;

    vector<bool>& a = vec_e_align_bit_array_[t];
    if (a.empty()) a.resize(end_pos_ - begin_pos_ + 1);
    a[i] = 1;

    align_.push_back({s, t});

    if (t > vec_right_most_[i]) vec_right_most_[i] = t;
    if (t < vec_left_most_[i]) vec_left_most_[i] = t;
  }

  /*
   * given a source span [begin, end], whether its target side is continuous,
   * return "0": the source span is translated silently
   * return "1": there is at least on word inside its target span, this word
   * doesn't align to any word inside [begin, end], but outside [begin, end]
   * return "2": otherwise
   */
  string IsTargetConstinousSpan(int begin, int end) const {
    int target_begin, target_end;
    FindLeftRightMostTargetSpan(begin, end, target_begin, target_end);
    if (target_begin == -1) return "0";

    for (int i = target_begin; i <= target_end; i++) {
      if (vec_e_align_bit_array_[i].empty()) continue;
      int j = begin;
      for (; j <= end; j++) {
        if (vec_e_align_bit_array_[i][j - begin_pos_]) break;
      }
      if (j == end + 1)  // e[i] is aligned, but e[i] doesn't align to any
                         // source word in [begin_pos, end_pos]
        return "1";
    }
    return "2";
  }

  string IsTargetConstinousSpan2(int begin, int end) const {
    int target_begin, target_end;
    FindLeftRightMostTargetSpan(begin, end, target_begin, target_end);
    if (target_begin == -1) return "Unaligned";

    for (int i = target_begin; i <= target_end; i++) {
      if (vec_e_align_bit_array_[i].empty()) continue;
      int j = begin;
      for (; j <= end; j++) {
        if (vec_e_align_bit_array_[i][j - begin_pos_]) break;
      }
      if (j == end + 1)  // e[i] is aligned, but e[i] doesn't align to any
                         // source word in [begin_pos, end_pos]
        return "Discon't";
    }
    return "Con't";
  }

  void FindLeftRightMostTargetSpan(int begin, int end, int& target_begin,
                                   int& target_end) const {
    int b = begin - begin_pos_;
    int e = end - begin_pos_ + 1;

    target_begin = vec_left_most_[b];
    target_end = vec_right_most_[b];
    for (int i = b + 1; i < e; i++) {
      if (target_begin > vec_left_most_[i]) target_begin = vec_left_most_[i];
      if (target_end < vec_right_most_[i]) target_end = vec_right_most_[i];
    }
    if (target_end == -1) target_begin = -1;
    return;

    target_begin = e_num_words_;
    target_end = -1;

    for (int i = begin - begin_pos_; i < end - begin_pos_ + 1; i++) {
      if (vec_f_align_bit_array_[i].empty()) continue;
      for (int j = 0; j < target_begin; j++)
        if (vec_f_align_bit_array_[i][j]) {
          target_begin = j;
          break;
        }
    }
    for (int i = end - begin_pos_; i > begin - begin_pos_ - 1; i--) {
      if (vec_f_align_bit_array_[i].empty()) continue;
      for (int j = e_num_words_ - 1; j > target_end; j--)
        if (vec_f_align_bit_array_[i][j]) {
          target_end = j;
          break;
        }
    }

    if (target_end == -1) target_begin = -1;
  }

  const uint16_t begin_pos_, end_pos_;              // the position in input
  const uint16_t e_num_words_;
  vector<AlignmentPoint> align_;

 private:
  vector<short> vec_left_most_;
  vector<short> vec_right_most_;
  vector<vector<bool> > vec_f_align_bit_array_;
  vector<vector<bool> > vec_e_align_bit_array_;
};

struct FocusedConstituent {
  FocusedConstituent(const SParsedTree* pTree) {
    if (pTree == NULL) return;
    for (size_t i = 0; i < pTree->m_vecTerminals.size(); i++) {
      STreeItem* pParent = pTree->m_vecTerminals[i]->m_ptParent;

      while (pParent != NULL) {
        // if (pParent->m_vecChildren.size() > 1 && pParent->m_iEnd -
        // pParent->m_iBegin > 5) {
        // if (pParent->m_vecChildren.size() > 1) {
        if (true) {

          // do constituent reordering for all children of pParent
          if (strcmp(pParent->m_pszTerm, "ROOT"))
            focus_parents_.push_back(pParent);
        }
        if (pParent->m_iBrotherIndex != 0) break;
        pParent = pParent->m_ptParent;
      }
    }
  }

  ~FocusedConstituent() {  // TODO
    focus_parents_.clear();
  }

  vector<STreeItem*> focus_parents_;
};

typedef SPredicateItem FocusedPredicate;

struct FocusedSRL {
  FocusedSRL(const SSrlSentence* srl) {
    if (srl == NULL) return;
    for (size_t i = 0; i < srl->m_vecPred.size(); i++) {
      if (strcmp(srl->m_pTree->m_vecTerminals[srl->m_vecPred[i]->m_iPosition]
                     ->m_ptParent->m_pszTerm,
                 "VA") == 0)
        continue;
      focus_predicates_.push_back(
          new FocusedPredicate(srl->m_pTree, srl->m_vecPred[i]));
    }
  }

  ~FocusedSRL() { focus_predicates_.clear(); }

  vector<const FocusedPredicate*> focus_predicates_;
};

struct ConstReorderFeatureImpl {
  ConstReorderFeatureImpl(const std::string& param) {

    b_block_feature_ = false;
    b_order_feature_ = false;
    b_srl_block_feature_ = false;
    b_srl_order_feature_ = false;

    vector<string> terms;
    SplitOnWhitespace(param, &terms);
    if (terms.size() == 1) {
      b_block_feature_ = true;
      b_order_feature_ = true;
    } else if (terms.size() >= 3) {
      if (terms[1].compare("1") == 0) b_block_feature_ = true;
      if (terms[2].compare("1") == 0) b_order_feature_ = true;
      if (terms.size() == 6) {
        if (terms[4].compare("1") == 0) b_srl_block_feature_ = true;
        if (terms[5].compare("1") == 0) b_srl_order_feature_ = true;

        assert(b_srl_block_feature_ || b_srl_order_feature_);
      }

    } else {
      assert("ERROR");
    }

    const_reorder_classifier_left_ = NULL;
    const_reorder_classifier_right_ = NULL;

    srl_reorder_classifier_left_ = NULL;
    srl_reorder_classifier_right_ = NULL;

    if (b_order_feature_) {
      InitializeClassifier((terms[0] + string(".left")).c_str(),
                           &const_reorder_classifier_left_);
      InitializeClassifier((terms[0] + string(".right")).c_str(),
                           &const_reorder_classifier_right_);
    }

    if (b_srl_order_feature_) {
      InitializeClassifier((terms[3] + string(".left")).c_str(),
                           &srl_reorder_classifier_left_);
      InitializeClassifier((terms[3] + string(".right")).c_str(),
                           &srl_reorder_classifier_right_);
    }

    parsed_tree_ = NULL;
    focused_consts_ = NULL;

    srl_sentence_ = NULL;
    focused_srl_ = NULL;

    map_left_ = NULL;
    map_right_ = NULL;

    map_srl_left_ = NULL;
    map_srl_right_ = NULL;

    dict_block_status_ = new Dict();
    dict_block_status_->Convert("Unaligned", false);
    dict_block_status_->Convert("Discon't", false);
    dict_block_status_->Convert("Con't", false);
  }

  ~ConstReorderFeatureImpl() {
    if (const_reorder_classifier_left_) delete const_reorder_classifier_left_;
    if (const_reorder_classifier_right_) delete const_reorder_classifier_right_;
    if (srl_reorder_classifier_left_) delete srl_reorder_classifier_left_;
    if (srl_reorder_classifier_right_) delete srl_reorder_classifier_right_;
    FreeSentenceVariables();

    delete dict_block_status_;
  }

  static int ReserveStateSize() { return 1 * sizeof(TargetTranslation*); }

  void InitializeInputSentence(const std::string& parse_file,
                               const std::string& srl_file) {
    FreeSentenceVariables();
    if (b_srl_block_feature_ || b_srl_order_feature_) {
      assert(srl_file != "");
      srl_sentence_ = ReadSRLSentence(srl_file);
      parsed_tree_ = srl_sentence_->m_pTree;
    } else {
      assert(parse_file != "");
      srl_sentence_ = NULL;
      parsed_tree_ = ReadParseTree(parse_file);
    }

    if (b_block_feature_ || b_order_feature_) {
      focused_consts_ = new FocusedConstituent(parsed_tree_);

      if (b_order_feature_) {
        // we can do the classifier "off-line"
        map_left_ = new MapClassifier();
        map_right_ = new MapClassifier();
        InitializeConstReorderClassifierOutput();
      }
    }

    if (b_srl_block_feature_ || b_srl_order_feature_) {
      focused_srl_ = new FocusedSRL(srl_sentence_);

      if (b_srl_order_feature_) {
        map_srl_left_ = new MapClassifier();
        map_srl_right_ = new MapClassifier();
        InitializeSRLReorderClassifierOutput();
      }
    }

    if (parsed_tree_ != NULL) {
      size_t i = parsed_tree_->m_vecTerminals.size();
      vec_target_tran_.reserve(20 * i * i * i);
    } else
      vec_target_tran_.reserve(1000000);
  }

  void SetConstReorderFeature(const Hypergraph::Edge& edge,
                              SparseVector<double>* features,
                              const vector<const void*>& ant_states,
                              void* state) {
    if (parsed_tree_ == NULL) return;

    short int begin = edge.i_, end = edge.j_ - 1;

    typedef TargetTranslation* PtrTargetTranslation;
    PtrTargetTranslation* remnant =
        reinterpret_cast<PtrTargetTranslation*>(state);

    vector<const TargetTranslation*> vec_node;
    vec_node.reserve(edge.tail_nodes_.size());
    for (size_t i = 0; i < edge.tail_nodes_.size(); i++) {
      const PtrTargetTranslation* astate =
          reinterpret_cast<const PtrTargetTranslation*>(ant_states[i]);
      vec_node.push_back(astate[0]);
    }

    int e_num_word = edge.rule_->e_.size();
    for (size_t i = 0; i < vec_node.size(); i++) {
      e_num_word += vec_node[i]->e_num_words_;
      e_num_word--;
    }

    remnant[0] = new TargetTranslation(begin, end, e_num_word);
    vec_target_tran_.push_back(remnant[0]);

    // reset the alignment
    // for the source side, we know its position in source sentence
    // for the target side, we always assume its starting position is 0
    unsigned vc = 0;
    const TRulePtr rule = edge.rule_;
    std::vector<int> f_index(rule->f_.size());
    int index = edge.i_;
    for (unsigned i = 0; i < rule->f_.size(); i++) {
      f_index[i] = index;
      const WordID& c = rule->f_[i];
      if (c < 1)
        index = vec_node[vc++]->end_pos_ + 1;
      else
        index++;
    }
    assert(vc == vec_node.size());
    assert(index == edge.j_);

    std::vector<int> e_index(rule->e_.size());
    index = 0;
    vc = 0;
    for (unsigned i = 0; i < rule->e_.size(); i++) {
      e_index[i] = index;
      const WordID& c = rule->e_[i];
      if (c < 1) {
        index += vec_node[-c]->e_num_words_;
        vc++;
      } else
        index++;
    }
    assert(vc == vec_node.size());

    size_t nt_pos = 0;
    for (size_t i = 0; i < edge.rule_->f_.size(); i++) {
      if (edge.rule_->f_[i] > 0) continue;

      // it's an NT
      size_t j;
      for (j = 0; j < edge.rule_->e_.size(); j++)
        if (edge.rule_->e_[j] * -1 == nt_pos) break;
      assert(j != edge.rule_->e_.size());
      nt_pos++;

      // i aligns j
      int eindex = e_index[j];
      const vector<AlignmentPoint>& align =
          vec_node[-1 * edge.rule_->e_[j]]->align_;
      for (size_t k = 0; k < align.size(); k++) {
        remnant[0]->InsertAlignmentPoint(align[k].s_, eindex + align[k].t_);
      }
    }
    for (size_t i = 0; i < edge.rule_->a_.size(); i++) {
      int findex = f_index[edge.rule_->a_[i].s_];
      int eindex = e_index[edge.rule_->a_[i].t_];
      remnant[0]->InsertAlignmentPoint(findex, eindex);
    }

    // till now, we finished setting state values
    // next, use the state values to calculate constituent reorder feature
    SetConstReorderFeature(begin, end, features, remnant[0],
                           vec_node, f_index);
  }

  void SetConstReorderFeature(short int begin, short int end,
                              SparseVector<double>* features,
                              const TargetTranslation* target_translation,
                              const vector<const TargetTranslation*>& vec_node,
                              std::vector<int>& /*findex*/) {
    if (b_srl_block_feature_ || b_srl_order_feature_) {
      double logprob_srl_reorder_left = 0.0, logprob_srl_reorder_right = 0.0;
      for (size_t i = 0; i < focused_srl_->focus_predicates_.size(); i++) {
        const FocusedPredicate* pred = focused_srl_->focus_predicates_[i];
        if (!is_overlap(begin, end, pred->begin_, pred->end_))
          continue;  // have no overlap between this predicate (with its
                     // argument) and the current edge

        size_t j;
        for (j = 0; j < vec_node.size(); j++) {
          if (is_inside(pred->begin_, pred->end_, vec_node[j]->begin_pos_,
                        vec_node[j]->end_pos_))
            break;
        }
        if (j < vec_node.size()) continue;

        vector<int> vecBlockStatus;
        vecBlockStatus.reserve(pred->vec_items_.size());
        for (j = 0; j < pred->vec_items_.size(); j++) {
          const STreeItem* con1 = pred->vec_items_[j]->tree_item_;
          if (con1->m_iBegin < begin || con1->m_iEnd > end) {
            vecBlockStatus.push_back(0);
            continue;
          }  // the node is partially outside the current edge

          string type = target_translation->IsTargetConstinousSpan2(
              con1->m_iBegin, con1->m_iEnd);
          vecBlockStatus.push_back(dict_block_status_->Convert(type, false));

          if (!b_srl_block_feature_) continue;
          // see if the node is covered by an NT
          size_t k;
          for (k = 0; k < vec_node.size(); k++) {
            if (is_inside(con1->m_iBegin, con1->m_iEnd, vec_node[k]->begin_pos_,
                          vec_node[k]->end_pos_))
              break;
          }
          if (k < vec_node.size()) continue;
          int f_id = FD::Convert(string(pred->vec_items_[j]->role_) + type);
          if (f_id) features->add_value(f_id, 1);
        }

        if (!b_srl_order_feature_) continue;

        vector<int> vecPosition, vecRelativePosition;
        vector<int> vecRightPosition, vecRelativeRightPosition;
        vecPosition.reserve(pred->vec_items_.size());
        vecRelativePosition.reserve(pred->vec_items_.size());
        vecRightPosition.reserve(pred->vec_items_.size());
        vecRelativeRightPosition.reserve(pred->vec_items_.size());
        for (j = 0; j < pred->vec_items_.size(); j++) {
          const STreeItem* con1 = pred->vec_items_[j]->tree_item_;
          if (con1->m_iBegin < begin || con1->m_iEnd > end) {
            vecPosition.push_back(-1);
            vecRightPosition.push_back(-1);
            continue;
          }  // the node is partially outside the current edge
          int left1 = -1, right1 = -1;
          target_translation->FindLeftRightMostTargetSpan(
              con1->m_iBegin, con1->m_iEnd, left1, right1);
          vecPosition.push_back(left1);
          vecRightPosition.push_back(right1);
        }
        fnGetRelativePosition(vecPosition, vecRelativePosition);
        fnGetRelativePosition(vecRightPosition, vecRelativeRightPosition);

        for (j = 1; j < pred->vec_items_.size(); j++) {
          const STreeItem* con1 = pred->vec_items_[j - 1]->tree_item_;
          const STreeItem* con2 = pred->vec_items_[j]->tree_item_;

          if (con1->m_iBegin < begin || con2->m_iEnd > end)
            continue;  // one of the two nodes is partially outside the current
                       // edge

          // both con1 and con2 are covered, need to check if they are covered
          // by the same NT
          size_t k;
          for (k = 0; k < vec_node.size(); k++) {
            if (is_inside(con1->m_iBegin, con2->m_iEnd, vec_node[k]->begin_pos_,
                          vec_node[k]->end_pos_))
              break;
          }
          if (k < vec_node.size()) continue;

          // they are not covered bye the same NT
          string outcome;
          string key;
          GenerateKey(pred->vec_items_[j - 1]->tree_item_,
                      pred->vec_items_[j]->tree_item_, vecBlockStatus[j - 1],
                      vecBlockStatus[j], key);

          fnGetOutcome(vecRelativePosition[j - 1], vecRelativePosition[j],
                       outcome);
          double prob = CalculateConstReorderProb(srl_reorder_classifier_left_,
                                                  map_srl_left_, key, outcome);
          // printf("%s %s %f\n", ostr.str().c_str(), outcome.c_str(), prob);
          logprob_srl_reorder_left += log10(prob);

          fnGetOutcome(vecRelativeRightPosition[j - 1],
                       vecRelativeRightPosition[j], outcome);
          prob = CalculateConstReorderProb(srl_reorder_classifier_right_,
                                           map_srl_right_, key, outcome);
          logprob_srl_reorder_right += log10(prob);
        }
      }

      if (b_srl_order_feature_) {
        int f_id = FD::Convert("SRLReorderFeatureLeft");
        if (f_id && logprob_srl_reorder_left != 0.0)
          features->set_value(f_id, logprob_srl_reorder_left);
        f_id = FD::Convert("SRLReorderFeatureRight");
        if (f_id && logprob_srl_reorder_right != 0.0)
          features->set_value(f_id, logprob_srl_reorder_right);
      }
    }

    if (b_block_feature_ || b_order_feature_) {
      double logprob_const_reorder_left = 0.0,
             logprob_const_reorder_right = 0.0;

      for (size_t i = 0; i < focused_consts_->focus_parents_.size(); i++) {
        STreeItem* parent = focused_consts_->focus_parents_[i];
        if (!is_overlap(begin, end, parent->m_iBegin,
                        parent->m_iEnd))
          continue;  // have no overlap between this parent node and the current
                     // edge

        size_t j;
        for (j = 0; j < vec_node.size(); j++) {
          if (is_inside(parent->m_iBegin, parent->m_iEnd,
                        vec_node[j]->begin_pos_, vec_node[j]->end_pos_))
            break;
        }
        if (j < vec_node.size()) continue;

        if (b_block_feature_) {
          if (parent->m_iBegin >= begin &&
              parent->m_iEnd <= end) {
            string type = target_translation->IsTargetConstinousSpan2(
                parent->m_iBegin, parent->m_iEnd);
            int f_id = FD::Convert(string(parent->m_pszTerm) + type);
            if (f_id) features->add_value(f_id, 1);
          }
        }

        if (parent->m_vecChildren.size() == 1 || !b_order_feature_) continue;

        vector<int> vecChunkBlock;
        vecChunkBlock.reserve(parent->m_vecChildren.size());

        for (j = 0; j < parent->m_vecChildren.size(); j++) {
          STreeItem* con1 = parent->m_vecChildren[j];
          if (con1->m_iBegin < begin || con1->m_iEnd > end) {
            vecChunkBlock.push_back(0);
            continue;
          }  // the node is partially outside the current edge

          string type = target_translation->IsTargetConstinousSpan2(
              con1->m_iBegin, con1->m_iEnd);
          vecChunkBlock.push_back(dict_block_status_->Convert(type, false));

          /*if (!b_block_feature_) continue;
          //see if the node is covered by an NT
          size_t k;
          for (k = 0; k < vec_node.size(); k++) {
                  if (is_inside(con1->m_iBegin, con1->m_iEnd,
          vec_node[k]->begin_pos_, vec_node[k]->end_pos_))
                          break;
          }
          if (k < vec_node.size()) continue;
          int f_id = FD::Convert(string(con1->m_pszTerm) + type);
          if (f_id)
                  features->add_value(f_id, 1);*/
        }

        if (!b_order_feature_) continue;

        vector<int> vecPosition, vecRelativePosition;
        vector<int> vecRightPosition, vecRelativeRightPosition;
        vecPosition.reserve(parent->m_vecChildren.size());
        vecRelativePosition.reserve(parent->m_vecChildren.size());
        vecRightPosition.reserve(parent->m_vecChildren.size());
        vecRelativeRightPosition.reserve(parent->m_vecChildren.size());
        for (j = 0; j < parent->m_vecChildren.size(); j++) {
          STreeItem* con1 = parent->m_vecChildren[j];
          if (con1->m_iBegin < begin || con1->m_iEnd > end) {
            vecPosition.push_back(-1);
            vecRightPosition.push_back(-1);
            continue;
          }  // the node is partially outside the current edge
          int left1 = -1, right1 = -1;
          target_translation->FindLeftRightMostTargetSpan(
              con1->m_iBegin, con1->m_iEnd, left1, right1);
          vecPosition.push_back(left1);
          vecRightPosition.push_back(right1);
        }
        fnGetRelativePosition(vecPosition, vecRelativePosition);
        fnGetRelativePosition(vecRightPosition, vecRelativeRightPosition);

        for (j = 1; j < parent->m_vecChildren.size(); j++) {
          STreeItem* con1 = parent->m_vecChildren[j - 1];
          STreeItem* con2 = parent->m_vecChildren[j];

          if (con1->m_iBegin < begin || con2->m_iEnd > end)
            continue;  // one of the two nodes is partially outside the current
                       // edge

          // both con1 and con2 are covered, need to check if they are covered
          // by the same NT
          size_t k;
          for (k = 0; k < vec_node.size(); k++) {
            if (is_inside(con1->m_iBegin, con2->m_iEnd, vec_node[k]->begin_pos_,
                          vec_node[k]->end_pos_))
              break;
          }
          if (k < vec_node.size()) continue;

          // they are not covered bye the same NT
          string outcome;
          string key;
          GenerateKey(parent->m_vecChildren[j - 1], parent->m_vecChildren[j],
                      vecChunkBlock[j - 1], vecChunkBlock[j], key);

          fnGetOutcome(vecRelativePosition[j - 1], vecRelativePosition[j],
                       outcome);
          double prob = CalculateConstReorderProb(
              const_reorder_classifier_left_, map_left_, key, outcome);
          // printf("%s %s %f\n", ostr.str().c_str(), outcome.c_str(), prob);
          logprob_const_reorder_left += log10(prob);

          fnGetOutcome(vecRelativeRightPosition[j - 1],
                       vecRelativeRightPosition[j], outcome);
          prob = CalculateConstReorderProb(const_reorder_classifier_right_,
                                           map_right_, key, outcome);
          logprob_const_reorder_right += log10(prob);
        }
      }

      if (b_order_feature_) {
        int f_id = FD::Convert("ConstReorderFeatureLeft");
        if (f_id && logprob_const_reorder_left != 0.0)
          features->set_value(f_id, logprob_const_reorder_left);
        f_id = FD::Convert("ConstReorderFeatureRight");
        if (f_id && logprob_const_reorder_right != 0.0)
          features->set_value(f_id, logprob_const_reorder_right);
      }
    }
  }

 private:
  void Byte_to_Char(unsigned char* str, int n) {
    str[0] = (n & 255);
    str[1] = n / 256;
  }
  void GenerateKey(const STreeItem* pCon1, const STreeItem* pCon2,
                   int iBlockStatus1, int iBlockStatus2, string& key) {
    assert(iBlockStatus1 != 0);
    assert(iBlockStatus2 != 0);
    unsigned char szTerm[1001];
    Byte_to_Char(szTerm, pCon1->m_iBegin);
    Byte_to_Char(szTerm + 2, pCon2->m_iEnd);
    szTerm[4] = (char)iBlockStatus1;
    szTerm[5] = (char)iBlockStatus2;
    szTerm[6] = '\0';
    // sprintf(szTerm, "%d|%d|%d|%d|%s|%s", pCon1->m_iBegin, pCon1->m_iEnd,
    // pCon2->m_iBegin, pCon2->m_iEnd, strBlockStatus1.c_str(),
    // strBlockStatus2.c_str());
    key = string(szTerm, szTerm + 6);
  }
  void InitializeConstReorderClassifierOutput() {
    if (!b_order_feature_) return;
    int size_block_status = dict_block_status_->max();

    for (size_t i = 0; i < focused_consts_->focus_parents_.size(); i++) {
      STreeItem* parent = focused_consts_->focus_parents_[i];

      for (size_t j = 1; j < parent->m_vecChildren.size(); j++) {
        for (size_t k = 1; k <= size_block_status; k++) {
          for (size_t l = 1; l <= size_block_status; l++) {
            ostringstream ostr;
            GenerateFeature(parsed_tree_, parent, j,
                            dict_block_status_->Convert(k),
                            dict_block_status_->Convert(l), ostr);

            string strKey;
            GenerateKey(parent->m_vecChildren[j - 1], parent->m_vecChildren[j],
                        k, l, strKey);

            vector<double> vecOutput;
            const_reorder_classifier_left_->fnEval(ostr.str().c_str(),
                                                   vecOutput);
            (*map_left_)[strKey] = vecOutput;

            const_reorder_classifier_right_->fnEval(ostr.str().c_str(),
                                                    vecOutput);
            (*map_right_)[strKey] = vecOutput;
          }
        }
      }
    }
  }

  void InitializeSRLReorderClassifierOutput() {
    if (!b_srl_order_feature_) return;
    int size_block_status = dict_block_status_->max();

    for (size_t i = 0; i < focused_srl_->focus_predicates_.size(); i++) {
      const FocusedPredicate* pred = focused_srl_->focus_predicates_[i];

      for (size_t j = 1; j < pred->vec_items_.size(); j++) {
        for (size_t k = 1; k <= size_block_status; k++) {
          for (size_t l = 1; l <= size_block_status; l++) {
            ostringstream ostr;

            SArgumentReorderModel::fnGenerateFeature(
                parsed_tree_, pred->pred_, pred, j,
                dict_block_status_->Convert(k), dict_block_status_->Convert(l),
                ostr);

            string strKey;
            GenerateKey(pred->vec_items_[j - 1]->tree_item_,
                        pred->vec_items_[j]->tree_item_, k, l, strKey);

            vector<double> vecOutput;
            srl_reorder_classifier_left_->fnEval(ostr.str().c_str(), vecOutput);
            (*map_srl_left_)[strKey] = vecOutput;

            srl_reorder_classifier_right_->fnEval(ostr.str().c_str(),
                                                  vecOutput);
            (*map_srl_right_)[strKey] = vecOutput;
          }
        }
      }
    }
  }

  double CalculateConstReorderProb(
      const Tsuruoka_Maxent* const_reorder_classifier, const MapClassifier* map,
      const string& key, const string& outcome) {
    MapClassifier::const_iterator iter = (*map).find(key);
    assert(iter != map->end());
    int id = const_reorder_classifier->fnGetClassId(outcome);
    return iter->second[id];
  }

  void FreeSentenceVariables() {
    if (srl_sentence_ != NULL) {
      delete srl_sentence_;
      srl_sentence_ = NULL;
    } else {
      if (parsed_tree_ != NULL) delete parsed_tree_;
      parsed_tree_ = NULL;
    }

    if (focused_consts_ != NULL) delete focused_consts_;
    focused_consts_ = NULL;

    for (size_t i = 0; i < vec_target_tran_.size(); i++)
      delete vec_target_tran_[i];
    vec_target_tran_.clear();

    if (map_left_ != NULL) delete map_left_;
    map_left_ = NULL;
    if (map_right_ != NULL) delete map_right_;
    map_right_ = NULL;

    if (map_srl_left_ != NULL) delete map_srl_left_;
    map_srl_left_ = NULL;
    if (map_srl_right_ != NULL) delete map_srl_right_;
    map_srl_right_ = NULL;
  }

  void InitializeClassifier(const char* pszFname,
                            Tsuruoka_Maxent** ppClassifier) {
    (*ppClassifier) = new Tsuruoka_Maxent(pszFname);
  }

  void GenerateOutcome(const vector<int>& vecPos, vector<string>& vecOutcome) {
    vecOutcome.clear();

    for (size_t i = 1; i < vecPos.size(); i++) {
      if (vecPos[i] == -1 || vecPos[i] == vecPos[i - 1]) {
        vecOutcome.push_back("M");  // monotone
        continue;
      }

      if (vecPos[i - 1] == -1) {
        // vecPos[i] is not -1
        size_t j = i - 2;
        while (j > -1 && vecPos[j] == -1) j--;

        size_t k;
        for (k = 0; k < j; k++) {
          if (vecPos[k] > vecPos[j] || vecPos[k] <= vecPos[i]) break;
        }
        if (k < j) {
          vecOutcome.push_back("DM");
          continue;
        }

        for (k = i + 1; k < vecPos.size(); k++)
          if (vecPos[k] < vecPos[i] && (j == -1 && vecPos[k] >= vecPos[j]))
            break;
        if (k < vecPos.size()) {
          vecOutcome.push_back("DM");
          continue;
        }
        vecOutcome.push_back("M");
      } else {
        // neither of vecPos[i-1] and vecPos[i] is -1
        if (vecPos[i - 1] < vecPos[i]) {
          // monotone or discon't monotone
          size_t j;
          for (j = 0; j < i - 1; j++)
            if (vecPos[j] > vecPos[i - 1] && vecPos[j] <= vecPos[i]) break;
          if (j < i - 1) {
            vecOutcome.push_back("DM");
            continue;
          }
          for (j = i + 1; j < vecPos.size(); j++)
            if (vecPos[j] >= vecPos[i - 1] && vecPos[j] < vecPos[i]) break;
          if (j < vecPos.size()) {
            vecOutcome.push_back("DM");
            continue;
          }
          vecOutcome.push_back("M");
        } else {
          // swap or discon't swap
          size_t j;
          for (j = 0; j < i - 1; j++)
            if (vecPos[j] > vecPos[i] && vecPos[j] <= vecPos[i - 1]) break;
          if (j < i - 1) {
            vecOutcome.push_back("DS");
            continue;
          }
          for (j = i + 1; j < vecPos.size(); j++)
            if (vecPos[j] >= vecPos[i] && vecPos[j] < vecPos[i - 1]) break;
          if (j < vecPos.size()) {
            vecOutcome.push_back("DS");
            continue;
          }
          vecOutcome.push_back("S");
        }
      }
    }

    assert(vecOutcome.size() == vecPos.size() - 1);
  }

  void fnGetRelativePosition(const vector<int>& vecLeft,
                             vector<int>& vecPosition) {
    vecPosition.clear();

    vector<float> vec;
    vec.reserve(vecLeft.size());
    for (size_t i = 0; i < vecLeft.size(); i++) {
      if (vecLeft[i] == -1) {
        if (i == 0)
          vec.push_back(-1);
        else
          vec.push_back(vecLeft[i - 1] + 0.1);
      } else
        vec.push_back(vecLeft[i]);
    }

    for (size_t i = 0; i < vecLeft.size(); i++) {
      int count = 0;

      for (size_t j = 0; j < vecLeft.size(); j++) {
        if (j == i) continue;
        if (vec[j] < vec[i]) {
          count++;
        } else if (vec[j] == vec[i] && j < i) {
          count++;
        }
      }
      vecPosition.push_back(count);
    }

    for (size_t i = 1; i < vecPosition.size(); i++) {
      if (vecPosition[i - 1] == vecPosition[i]) {
        for (size_t j = 0; j < vecLeft.size(); j++) cout << vecLeft[j] << " ";
        cout << "\n";
        assert(false);
      }
    }
  }

  inline void fnGetOutcome(int i1, int i2, string& strOutcome) {
    assert(i1 != i2);
    if (i1 < i2) {
      if (i2 > i1 + 1)
        strOutcome = string("DM");
      else
        strOutcome = string("M");
    } else {
      if (i1 > i2 + 1)
        strOutcome = string("DS");
      else
        strOutcome = string("S");
    }
  }

  // features in constituent_reorder_model.cc
  void GenerateFeature(const SParsedTree* pTree, const STreeItem* pParent,
                       int iPos, const string& strBlockStatus1,
                       const string& strBlockStatus2, ostringstream& ostr) {
    STreeItem* pCon1, *pCon2;
    pCon1 = pParent->m_vecChildren[iPos - 1];
    pCon2 = pParent->m_vecChildren[iPos];

    string left_label = string(pCon1->m_pszTerm);
    string right_label = string(pCon2->m_pszTerm);
    string parent_label = string(pParent->m_pszTerm);

    vector<string> vec_other_right_sibling;
    for (int i = iPos + 1; i < pParent->m_vecChildren.size(); i++)
      vec_other_right_sibling.push_back(
          string(pParent->m_vecChildren[i]->m_pszTerm));
    if (vec_other_right_sibling.size() == 0)
      vec_other_right_sibling.push_back(string("NULL"));
    vector<string> vec_other_left_sibling;
    for (int i = 0; i < iPos - 1; i++)
      vec_other_left_sibling.push_back(
          string(pParent->m_vecChildren[i]->m_pszTerm));
    if (vec_other_left_sibling.size() == 0)
      vec_other_left_sibling.push_back(string("NULL"));

    // generate features
    // f1
    ostr << "f1=" << left_label << "_" << right_label << "_" << parent_label;
    // f2
    for (int i = 0; i < vec_other_right_sibling.size(); i++)
      ostr << " f2=" << left_label << "_" << right_label << "_" << parent_label
           << "_" << vec_other_right_sibling[i];
    // f3
    for (int i = 0; i < vec_other_left_sibling.size(); i++)
      ostr << " f3=" << left_label << "_" << right_label << "_" << parent_label
           << "_" << vec_other_left_sibling[i];
    // f4
    ostr << " f4=" << left_label << "_" << right_label << "_"
         << pTree->m_vecTerminals[pCon1->m_iHeadWord]->m_ptParent->m_pszTerm;
    // f5
    ostr << " f5=" << left_label << "_" << right_label << "_"
         << pTree->m_vecTerminals[pCon1->m_iHeadWord]->m_pszTerm;
    // f6
    ostr << " f6=" << left_label << "_" << right_label << "_"
         << pTree->m_vecTerminals[pCon2->m_iHeadWord]->m_ptParent->m_pszTerm;
    // f7
    ostr << " f7=" << left_label << "_" << right_label << "_"
         << pTree->m_vecTerminals[pCon2->m_iHeadWord]->m_pszTerm;
    // f8
    ostr << " f8=" << left_label << "_" << right_label << "_"
         << strBlockStatus1;
    // f9
    ostr << " f9=" << left_label << "_" << right_label << "_"
         << strBlockStatus2;

    // f10
    ostr << " f10=" << left_label << "_" << parent_label;
    // f11
    ostr << " f11=" << right_label << "_" << parent_label;
  }

  SParsedTree* ReadParseTree(const std::string& parse_file) {
    SParseReader* reader = new SParseReader(parse_file.c_str(), false);
    SParsedTree* tree = reader->fnReadNextParseTree();
    // assert(tree != NULL);
    delete reader;
    return tree;
  }

  SSrlSentence* ReadSRLSentence(const std::string& srl_file) {
    SSrlSentenceReader* reader = new SSrlSentenceReader(srl_file.c_str());
    SSrlSentence* srl = reader->fnReadNextSrlSentence();
    // assert(srl != NULL);
    delete reader;
    return srl;
  }

 private:
  Tsuruoka_Maxent* const_reorder_classifier_left_;
  Tsuruoka_Maxent* const_reorder_classifier_right_;

  Tsuruoka_Maxent* srl_reorder_classifier_left_;
  Tsuruoka_Maxent* srl_reorder_classifier_right_;

  MapClassifier* map_left_;
  MapClassifier* map_right_;

  MapClassifier* map_srl_left_;
  MapClassifier* map_srl_right_;

  SParsedTree* parsed_tree_;
  FocusedConstituent* focused_consts_;
  vector<TargetTranslation*> vec_target_tran_;

  bool b_order_feature_;
  bool b_block_feature_;

  bool b_srl_block_feature_;
  bool b_srl_order_feature_;
  SSrlSentence* srl_sentence_;
  FocusedSRL* focused_srl_;

  Dict* dict_block_status_;
};

ConstReorderFeature::ConstReorderFeature(const std::string& param) {
  pimpl_ = new ConstReorderFeatureImpl(param);
  SetStateSize(ConstReorderFeatureImpl::ReserveStateSize());
  SetIgnoredStateSize(ConstReorderFeatureImpl::ReserveStateSize());
  name_ = "ConstReorderFeature";
}

ConstReorderFeature::~ConstReorderFeature() {  // TODO
  delete pimpl_;
}

void ConstReorderFeature::PrepareForInput(const SentenceMetadata& smeta) {
  string parse_file = smeta.GetSGMLValue("parse");
  if (parse_file.empty()) {
    parse_file = smeta.GetSGMLValue("src_tree");
  }
  string srl_file = smeta.GetSGMLValue("srl");
  assert(!(parse_file == "" && srl_file == ""));

  pimpl_->InitializeInputSentence(parse_file, srl_file);
}

void ConstReorderFeature::TraversalFeaturesImpl(
    const SentenceMetadata& /* smeta */, const Hypergraph::Edge& edge,
    const vector<const void*>& ant_states, SparseVector<double>* features,
    SparseVector<double>* /*estimated_features*/, void* state) const {
  pimpl_->SetConstReorderFeature(edge, features, ant_states, state);
}

string ConstReorderFeature::usage(bool show_params, bool show_details) {
  ostringstream out;
  out << "ConstReorderFeature";
  if (show_params) {
    out << " model_file_prefix [const_block=1 const_order=1] [srl_block=0 "
           "srl_order=0]"
        << "\nParameters:\n"
        << "  const_{block,order}: enable/disable constituency constraints.\n"
        << "  src_{block,order}: enable/disable semantic role labeling "
           "constraints.\n";
  }
  if (show_details) {
    out << "\n"
        << "Soft reordering constraint features from "
           "http://www.aclweb.org/anthology/P14-1106. To train the classifers, "
           "use utils/const_reorder_model_trainer for constituency reordering "
           "constraints and utils/argument_reorder_model_trainer for semantic "
           "role labeling reordering constraints.\n"
        << "Input segments should provide path to parse tree (resp. SRL parse) "
           "as \"parse\" (resp. \"srl\") properties.\n";
  }
  return out.str();
}

boost::shared_ptr<FeatureFunction> CreateConstReorderModel(
    const std::string& param) {
  ConstReorderFeature* ret = new ConstReorderFeature(param);
  return boost::shared_ptr<FeatureFunction>(ret);
}
