/*
 * ff_soft_syn.cc
 *
 */
#include "ff_soft_syn.h"

#include "filelib.h"
#include "stringlib.h"
#include "hg.h"
#include "sentence_metadata.h"
#include "tree.h"

#include <string>
#include <vector>
#include <stdio.h>

using namespace std;

typedef HASH_MAP<std::string, vector<string> > MapFeatures;

/*
 * Note:
 *      In BOLT experiments, we need to merged some sequence words into one term
 *(like from "1999 nian 1 yue 10 ri" to "1999_nian_1_yue_10_ri") due to some
 *reasons;
 *      but in the parse file, we still use the parse tree before merging any
 *words;
 *      therefore, the words in source sentence and parse tree diverse and we
 *need to map a word in merged sentence into its original index;
 *      a word in source sentence maps 1 or more words in parse tree
 *      the index map info is stored at variable index_map_;
 *      if the index_map_ is NULL, indicating the word index in source sentence
 *and parse tree is always same.
 *
 */

struct SoftSynFeatureImpl {
  SoftSynFeatureImpl(const string& /*params*/) {
    parsed_tree_ = NULL;
    index_map_ = NULL;

    map_features_ = NULL;
  }

  ~SoftSynFeatureImpl() { FreeSentenceVariables(); }

  static int ReserveStateSize() { return 2 * sizeof(uint16_t); }

  void InitializeInputSentence(const std::string& parse_file,
                               const std::string& index_map_file) {
    FreeSentenceVariables();
    parsed_tree_ = ReadParseTree(parse_file);

    if (index_map_file != "") ReadIndexMap(index_map_file);

    // we can do the features "off-line"
    map_features_ = new MapFeatures();
    InitializeFeatures(map_features_);
  }

  void ReadIndexMap(const std::string& index_map_file) {
    vector<string> terms;
    {
      ReadFile file(index_map_file);
      string line;
      assert(getline(*file.stream(), line));
      SplitOnWhitespace(line, &terms);
    }

    index_map_ = new short int[terms.size() + 1];
    int ix = 0;
    size_t i;
    for (i = 0; i < terms.size(); i++) {
      index_map_[i] = ix;
      ix += atoi(terms[i].c_str());
    }
    index_map_[i] = ix;
    assert(parsed_tree_ == NULL || ix == parsed_tree_->m_vecTerminals.size());
  }

  void MapIndex(short int begin, short int end, short int& mapped_begin,
                short int& mapped_end) {
    if (index_map_ == NULL) {
      mapped_begin = begin;
      mapped_end = end;
      return;
    }

    mapped_begin = index_map_[begin];
    mapped_end = index_map_[end + 1] - 1;
  }

  /*
   * ff_const_reorder.cc::ConstReorderFeatureImpl also defines this function
   */
  void FindConsts(const SParsedTree* tree, int begin, int end,
                  vector<STreeItem*>& consts) {
    STreeItem* item;
    item = tree->m_vecTerminals[begin]->m_ptParent;
    while (true) {
      while (item->m_ptParent != NULL &&
             item->m_ptParent->m_iBegin == item->m_iBegin &&
             item->m_ptParent->m_iEnd <= end)
        item = item->m_ptParent;

      if (item->m_ptParent == NULL && item->m_vecChildren.size() == 1 &&
          strcmp(item->m_pszTerm, "ROOT") == 0)
        item = item->m_vecChildren[0];  // we automatically add a "ROOT" node at
                                        // the top, skip it if necessary.

      consts.push_back(item);
      if (item->m_iEnd < end)
        item = tree->m_vecTerminals[item->m_iEnd + 1]->m_ptParent;
      else
        break;
    }
  }

  /*
   * according to Marton & Resnik (2008)
   * a span cann't have both X+ style and X= style features
   * a constituent XP is crossed only if the span not only covers parts of XP's
   *content, but also covers one or more words outside XP
   * a span may have X+, Y+
   *
   * (note, we refer X* features to X= features in Marton & Resnik (2008))
   */
  void GenerateSoftFeature(int begin, int end, const SParsedTree* tree,
                           vector<string>& vecFeature) {
    vector<STreeItem*> vecNode;
    FindConsts(tree, begin, end, vecNode);

    if (vecNode.size() == 1) {
      // match to one constituent
      string feature_name = string(vecNode[0]->m_pszTerm) + string("*");
      vecFeature.push_back(feature_name);
    } else {
      // match to multiple constituents, find the lowest common parent (lcp)
      STreeItem* lcp = vecNode[0];
      while (lcp->m_iEnd < end) lcp = lcp->m_ptParent;

      for (size_t i = 0; i < vecNode.size(); i++) {
        STreeItem* item = vecNode[i];

        while (item != lcp) {
          if (item->m_iBegin < begin || item->m_iEnd > end) {
            // item is crossed
            string feature_name = string(item->m_pszTerm) + string("+");
            vecFeature.push_back(feature_name);
          }
          if (item->m_iBrotherIndex > 0 &&
              item->m_ptParent->m_vecChildren[item->m_iBrotherIndex - 1]
                      ->m_iBegin >= begin &&
              item->m_ptParent->m_vecChildren[item->m_iBrotherIndex - 1]
                      ->m_iEnd <= end)
            break;  // we don't want to collect crossed constituents twice
          item = item->m_ptParent;
        }
      }
    }
  }

  void GenerateSoftFeatureFromFlattenedTree(int begin, int end,
                                            const SParsedTree* tree,
                                            vector<string>& vecFeature) {
    vector<STreeItem*> vecNode;
    FindConsts(tree, begin, end, vecNode);

    if (vecNode.size() == 1) {
      // match to one constituent
      string feature_name = string(vecNode[0]->m_pszTerm) + string("*");
      vecFeature.push_back(feature_name);
    } else {
      // match to multiple constituents, see if they have a common parent
      size_t i = 0;
      for (i = 1; i < vecNode.size(); i++) {
        if (vecNode[i]->m_ptParent != vecNode[0]->m_ptParent) break;
      }
      if (i == vecNode.size()) {
        // they share a common parent
        string feature_name =
            string(vecNode[0]->m_ptParent->m_pszTerm) + string("&");
        vecFeature.push_back(feature_name);
      } else {
        // they don't share a common parent, find the lowest common parent (lcp)
        STreeItem* lcp = vecNode[0];
        while (lcp->m_iEnd < end) lcp = lcp->m_ptParent;

        for (size_t i = 0; i < vecNode.size(); i++) {
          STreeItem* item = vecNode[i];

          while (item != lcp) {
            if (item->m_iBegin < begin || item->m_iEnd > end) {
              // item is crossed
              string feature_name = string(item->m_pszTerm) + string("+");
              vecFeature.push_back(feature_name);
            }
            if (item->m_iBrotherIndex > 0 &&
                item->m_ptParent->m_vecChildren[item->m_iBrotherIndex - 1]
                        ->m_iBegin >= begin &&
                item->m_ptParent->m_vecChildren[item->m_iBrotherIndex - 1]
                        ->m_iEnd <= end)
              break;  // we don't want to collect crossed constituents twice
            item = item->m_ptParent;
          }
        }
      }
    }
  }

  void SetSoftSynFeature(const Hypergraph::Edge& edge,
                         SparseVector<double>* features,
                         const vector<const void*>& ant_states, void* state) {
    vector<uint16_t> vec_pos;
    if (parsed_tree_ == NULL) return;

    uint16_t* remnant = reinterpret_cast<uint16_t*>(state);

    short int mapped_begin, mapped_end;
    MapIndex(edge.i_, edge.j_ - 1, mapped_begin, mapped_end);

    remnant[0] = mapped_begin;
    remnant[1] = mapped_end;

    for (size_t i = 0; i < edge.tail_nodes_.size(); i++) {
      const uint16_t* astate = reinterpret_cast<const uint16_t*>(ant_states[i]);
      vec_pos.push_back(astate[0]);
      vec_pos.push_back(astate[1]);
    }

    // soft feature for the whole span
    const vector<string> vecFeature =
        GenerateSoftFeature(mapped_begin, mapped_end, map_features_);
    for (size_t i = 0; i < vecFeature.size(); i++) {
      int f_id = FD::Convert(vecFeature[i]);
      if (f_id) features->set_value(f_id, 1);
    }
  }

 private:
  const vector<string>& GenerateSoftFeature(int begin, int end,
                                            MapFeatures* map_features) {
    string key;
    GenerateKey(begin, end, key);
    MapFeatures::const_iterator iter = (*map_features).find(key);
    assert(iter != map_features->end());
    return iter->second;
  }

  void Byte_to_Char(unsigned char* str, int n) {
    str[0] = (n & 255);
    str[1] = n / 256;
  }

  void GenerateKey(int begin, int end, string& key) {
    unsigned char szTerm[1001];
    Byte_to_Char(szTerm, begin);
    Byte_to_Char(szTerm + 2, end);
    szTerm[4] = '\0';
    key = string(szTerm, szTerm + 4);
  }

  void InitializeFeatures(MapFeatures* map_features) {
    if (parsed_tree_ == NULL) return;

    for (size_t i = 0; i < parsed_tree_->m_vecTerminals.size(); i++)
      for (size_t j = i; j < parsed_tree_->m_vecTerminals.size(); j++) {
        vector<string> vecFeature;
        GenerateSoftFeature(i, j, parsed_tree_, vecFeature);
        string key;
        GenerateKey(i, j, key);
        (*map_features)[key] = vecFeature;
      }
  }

  void FreeSentenceVariables() {
    if (parsed_tree_ != NULL) delete parsed_tree_;
    if (index_map_ != NULL) delete[] index_map_;
    index_map_ = NULL;

    if (map_features_ != NULL) delete map_features_;
  }

  SParsedTree* ReadParseTree(const std::string& parse_file) {
    SParseReader* reader = new SParseReader(parse_file.c_str(), false);
    SParsedTree* tree = reader->fnReadNextParseTree();
    // assert(tree != NULL);
    delete reader;
    return tree;
  }

 private:
  SParsedTree* parsed_tree_;

  short int* index_map_;

  MapFeatures* map_features_;
};

SoftSynFeature::SoftSynFeature(std::string param) {
  pimpl_ = new SoftSynFeatureImpl(param);
  SetStateSize(SoftSynFeatureImpl::ReserveStateSize());
  name_ = "SoftSynFeature";
}

SoftSynFeature::~SoftSynFeature() { delete pimpl_; }

void SoftSynFeature::PrepareForInput(const SentenceMetadata& smeta) {
  string parse_file = smeta.GetSGMLValue("parse");
  assert(parse_file != "");

  string indexmap_file = smeta.GetSGMLValue("index-map");

  pimpl_->InitializeInputSentence(parse_file, indexmap_file);
}

void SoftSynFeature::TraversalFeaturesImpl(
    const SentenceMetadata& /* smeta */, const Hypergraph::Edge& edge,
    const vector<const void*>& ant_states, SparseVector<double>* features,
    SparseVector<double>* /*estimated_features*/, void* state) const {
  pimpl_->SetSoftSynFeature(edge, features, ant_states, state);
}

string SoftSynFeature::usage(bool /*param*/, bool /*verbose*/) {
  return "SoftSynFeature";
}

boost::shared_ptr<FeatureFunction> CreateSoftSynFeatureModel(
    std::string param) {
  SoftSynFeature* ret = new SoftSynFeature(param);
  return boost::shared_ptr<FeatureFunction>(ret);
}

boost::shared_ptr<FeatureFunction> SoftSynFeatureFactory::Create(
    std::string param) const {
  return CreateSoftSynFeatureModel(param);
}

std::string SoftSynFeatureFactory::usage(bool params, bool verbose) const {
  return SoftSynFeature::usage(params, verbose);
}

typedef HASH_MAP<std::string, double> MapDouble;
typedef HASH_MAP<std::string, MapDouble*> MapDoubleFeatures;

/*
 * Note:
 *      In BOLT experiments, we need to merged some sequence words into one term
 *(like from "1999 nian 1 yue 10 ri" to "1999_nian_1_yue_10_ri") due to some
 *reasons;
 *      but in the parse file, we still use the parse tree before merging any
 *words;
 *      therefore, the words in source sentence and parse tree diverse and we
 *need to map a word in merged sentence into its original index;
 *      a word in source sentence maps 1 or more words in parse tree
 *      the index map info is stored at variable index_map_;
 *      if the index_map_ is NULL, indicating the word index in source sentence
 *and parse tree is always same.
 *
 */

struct SoftKBestSynFeatureImpl {
  SoftKBestSynFeatureImpl(const string& /*params*/) {
    index_map_ = NULL;

    map_features_ = NULL;
  }

  ~SoftKBestSynFeatureImpl() { FreeSentenceVariables(); }

  static int ReserveStateSize() { return 2 * sizeof(uint16_t); }

  void InitializeInputSentence(const std::string& parse_file,
                               const std::string& index_map_file) {
    FreeSentenceVariables();
    ReadParseTree(parse_file, vec_parsed_tree_, vec_tree_prob_);

    if (index_map_file != "") ReadIndexMap(index_map_file);

    // we can do the features "off-line"
    map_features_ = new MapDoubleFeatures();
    InitializeFeatures(map_features_);
  }

  void SetSoftKBestSynFeature(const Hypergraph::Edge& edge,
                              SparseVector<double>* features,
                              const vector<const void*>& ant_states,
                              void* state) {
    vector<uint16_t> vec_pos;
    if (vec_parsed_tree_.size() == 0) return;

    uint16_t* remnant = reinterpret_cast<uint16_t*>(state);

    short int mapped_begin, mapped_end;
    MapIndex(edge.i_, edge.j_ - 1, mapped_begin, mapped_end);

    remnant[0] = mapped_begin;
    remnant[1] = mapped_end;

    for (size_t i = 0; i < edge.tail_nodes_.size(); i++) {
      const uint16_t* astate = reinterpret_cast<const uint16_t*>(ant_states[i]);
      vec_pos.push_back(astate[0]);
      vec_pos.push_back(astate[1]);
    }

    // soft feature for the whole span
    const MapDouble* pMapFeature =
        GenerateSoftFeature(mapped_begin, mapped_end, map_features_);
    for (MapDouble::const_iterator iter = pMapFeature->begin();
         iter != pMapFeature->end(); iter++) {
      int f_id = FD::Convert(iter->first);
      if (f_id) features->set_value(f_id, iter->second);
    }
  }

 private:
  void ReadIndexMap(const std::string& index_map_file) {
    vector<string> terms;
    {
      ReadFile file(index_map_file);
      string line;
      assert(getline(*file.stream(), line));
      SplitOnWhitespace(line, &terms);
    }

    index_map_ = new short int[terms.size() + 1];
    int ix = 0;
    size_t i;
    for (i = 0; i < terms.size(); i++) {
      index_map_[i] = ix;
      ix += atoi(terms[i].c_str());
    }
    index_map_[i] = ix;
    assert(vec_parsed_tree_.size() == 0 ||
           ix == vec_parsed_tree_[0]->m_vecTerminals.size());
  }

  void MapIndex(short int begin, short int end, short int& mapped_begin,
                short int& mapped_end) {
    if (index_map_ == NULL) {
      mapped_begin = begin;
      mapped_end = end;
      return;
    }

    mapped_begin = index_map_[begin];
    mapped_end = index_map_[end + 1] - 1;
  }

  /*
   * ff_const_reorder.cc::ConstReorderFeatureImpl also defines this function
   */
  void FindConsts(const SParsedTree* tree, int begin, int end,
                  vector<STreeItem*>& consts) {
    STreeItem* item;
    item = tree->m_vecTerminals[begin]->m_ptParent;
    while (true) {
      while (item->m_ptParent != NULL &&
             item->m_ptParent->m_iBegin == item->m_iBegin &&
             item->m_ptParent->m_iEnd <= end)
        item = item->m_ptParent;

      if (item->m_ptParent == NULL && item->m_vecChildren.size() == 1 &&
          strcmp(item->m_pszTerm, "ROOT") == 0)
        item = item->m_vecChildren[0];  // we automatically add a "ROOT" node at
                                        // the top, skip it if necessary.

      consts.push_back(item);
      if (item->m_iEnd < end)
        item = tree->m_vecTerminals[item->m_iEnd + 1]->m_ptParent;
      else
        break;
    }
  }

  /*
   * according to Marton & Resnik (2008)
   * a span cann't have both X+ style and X= style features
   * a constituent XP is crossed only if the span not only covers parts of XP's
   *content, but also covers one or more words outside XP
   * a span may have X+, Y+
   *
   * (note, we refer X* features to X= features in Marton & Resnik (2008))
   */
  void GenerateSoftFeature(int begin, int end,
                           const vector<SParsedTree*>& vec_tree,
                           const vector<double>& vec_prob,
                           MapDouble* pMapFeature) {

    for (size_t i = 0; i < vec_tree.size(); i++) {
      const SParsedTree* tree = vec_tree[i];
      vector<STreeItem*> vecNode;
      FindConsts(tree, begin, end, vecNode);

      if (vecNode.size() == 1) {
        // match to one constituent
        string feature_name = string(vecNode[0]->m_pszTerm) + string("*");
        MapDouble::iterator iter = pMapFeature->find(feature_name);
        if (iter != pMapFeature->end()) {
          iter->second += vec_prob[i];
        } else
          (*pMapFeature)[feature_name] = vec_prob[i];
      } else {
        // match to multiple constituents, find the lowest common parent (lcp)
        STreeItem* lcp = vecNode[0];
        while (lcp->m_iEnd < end) lcp = lcp->m_ptParent;

        for (size_t j = 0; j < vecNode.size(); j++) {
          STreeItem* item = vecNode[j];

          while (item != lcp) {
            if (item->m_iBegin < begin || item->m_iEnd > end) {
              // item is crossed
              string feature_name = string(item->m_pszTerm) + string("+");
              MapDouble::iterator iter = pMapFeature->find(feature_name);
              if (iter != pMapFeature->end()) {
                iter->second += vec_prob[i];
              } else
                (*pMapFeature)[feature_name] = vec_prob[i];
            }
            if (item->m_iBrotherIndex > 0 &&
                item->m_ptParent->m_vecChildren[item->m_iBrotherIndex - 1]
                        ->m_iBegin >= begin &&
                item->m_ptParent->m_vecChildren[item->m_iBrotherIndex - 1]
                        ->m_iEnd <= end)
              break;  // we don't want to collect crossed constituents twice
            item = item->m_ptParent;
          }
        }
      }
    }
  }

  const MapDouble* GenerateSoftFeature(int begin, int end,
                                       MapDoubleFeatures* map_features) {
    string key;
    GenerateKey(begin, end, key);
    MapDoubleFeatures::const_iterator iter = (*map_features).find(key);
    assert(iter != map_features->end());
    return iter->second;
  }

  void Byte_to_Char(unsigned char* str, int n) {
    str[0] = (n & 255);
    str[1] = n / 256;
  }

  void GenerateKey(int begin, int end, string& key) {
    unsigned char szTerm[1001];
    Byte_to_Char(szTerm, begin);
    Byte_to_Char(szTerm + 2, end);
    szTerm[4] = '\0';
    key = string(szTerm, szTerm + 4);
  }

  void InitializeFeatures(MapDoubleFeatures* map_features) {
    if (vec_parsed_tree_.size() == 0) return;

    const SParsedTree* pTree = vec_parsed_tree_[0];

    vector<double> vec_prob;
    vec_prob.reserve(vec_tree_prob_.size());
    double tmp = 0.0;
    for (size_t i = 0; i < vec_tree_prob_.size(); i++) {
      vec_prob.push_back(pow(10, vec_tree_prob_[i] - vec_tree_prob_[0]));
      tmp += vec_prob[i];
    }
    for (size_t i = 0; i < vec_prob.size(); i++) vec_prob[i] /= tmp;

    for (size_t i = 0; i < pTree->m_vecTerminals.size(); i++)
      for (size_t j = i; j < pTree->m_vecTerminals.size(); j++) {
        MapDouble* pMap = new MapDouble();
        GenerateSoftFeature(i, j, vec_parsed_tree_, vec_prob, pMap);
        string key;
        GenerateKey(i, j, key);
        (*map_features)[key] = pMap;
      }
  }

  void FreeSentenceVariables() {
    for (size_t i = 0; i < vec_parsed_tree_.size(); i++) {
      if (vec_parsed_tree_[i] != NULL) delete vec_parsed_tree_[i];
    }
    vec_parsed_tree_.clear();
    vec_tree_prob_.clear();
    if (index_map_ != NULL) delete[] index_map_;
    index_map_ = NULL;

    if (map_features_ != NULL) {
      for (MapDoubleFeatures::iterator iter = map_features_->begin();
           iter != map_features_->end(); iter++)
        delete iter->second;
      delete map_features_;
    }
  }

  void ReadParseTree(const std::string& parse_file,
                     vector<SParsedTree*>& vec_tree, vector<double>& vec_prob) {
    ReadFile in(parse_file);
    SParsedTree* tree;
    string line;
    while (getline(*in.stream(), line)) {
      const char* p = strchr(line.c_str(), ' ');
      assert(p != NULL);
      string strProb = line.substr(0, line.find(' '));
      tree = SParsedTree::fnConvertFromString(p + 1);
      tree->fnSetSpanInfo();
      tree->fnSetHeadWord();
      vec_tree.push_back(tree);
      if (strProb == string("-Infinity")) {
        vec_prob.push_back(-99.0);
        break;
      } else {
        vec_prob.push_back(atof(strProb.c_str()));
      }
    }
  }

  void ReadParseTree2(const std::string& parse_file,
                      vector<SParsedTree*>& vec_tree,
                      vector<double>& vec_prob) {
    SParseReader* reader = new SParseReader(parse_file.c_str(), false);
    double prob;
    SParsedTree* tree;
    while ((tree = reader->fnReadNextParseTreeWithProb(&prob)) != NULL) {
      vec_tree.push_back(tree);
      if (std::isinf(prob)) {
        vec_prob.push_back(-99);
        break;
      } else
        vec_prob.push_back(prob);
    }
    // assert(tree != NULL);
    delete reader;
  }

 private:
  vector<SParsedTree*> vec_parsed_tree_;
  vector<double> vec_tree_prob_;

  short int* index_map_;

  MapDoubleFeatures* map_features_;
};

SoftKBestSynFeature::SoftKBestSynFeature(std::string param) {
  pimpl_ = new SoftKBestSynFeatureImpl(param);
  SetStateSize(SoftKBestSynFeatureImpl::ReserveStateSize());
  name_ = "SoftKBestSynFeature";
}

SoftKBestSynFeature::~SoftKBestSynFeature() { delete pimpl_; }

void SoftKBestSynFeature::PrepareForInput(const SentenceMetadata& smeta) {
  string parse_file = smeta.GetSGMLValue("kbestparse");
  assert(parse_file != "");

  string indexmap_file = smeta.GetSGMLValue("index-map");

  pimpl_->InitializeInputSentence(parse_file, indexmap_file);
}

void SoftKBestSynFeature::TraversalFeaturesImpl(
    const SentenceMetadata& /* smeta */, const Hypergraph::Edge& edge,
    const vector<const void*>& ant_states, SparseVector<double>* features,
    SparseVector<double>* /*estimated_features*/, void* state) const {
  pimpl_->SetSoftKBestSynFeature(edge, features, ant_states, state);
}

string SoftKBestSynFeature::usage(bool /*param*/, bool /*verbose*/) {
  return "SoftKBestSynFeature";
}

boost::shared_ptr<FeatureFunction> CreateSoftKBestSynFeatureModel(
    std::string param) {
  SoftKBestSynFeature* ret = new SoftKBestSynFeature(param);
  return boost::shared_ptr<FeatureFunction>(ret);
}

boost::shared_ptr<FeatureFunction> SoftKBestSynFeatureFactory::Create(
    std::string param) const {
  return CreateSoftKBestSynFeatureModel(param);
}

std::string SoftKBestSynFeatureFactory::usage(bool params, bool verbose) const {
  return SoftKBestSynFeature::usage(params, verbose);
}
