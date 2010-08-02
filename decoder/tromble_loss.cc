#include "tromble_loss.h"
#include "fast_lexical_cast.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/functional/hash.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/tokenizer.hpp>
#include <boost/unordered_map.hpp>

#include <cmath>
#include <fstream>
#include <vector>

#include "sentence_metadata.h"
#include "trule.h"
#include "tdict.h"

using namespace std;

namespace {

typedef unsigned char GramCount;

struct RefCounts {
  GramCount max;
  std::vector<GramCount> refs;
  size_t length;
};

typedef boost::unordered_map<std::vector<WordID>, size_t, boost::hash<std::vector<WordID> > > NGramMap;

// Take all the n-grams in the references and stuff them into ngrams.
void MakeNGramMapFromReferences(const vector<vector<WordID> > &references,
                                int n,
                                vector<RefCounts> *counts,
                                NGramMap *ngrams) {
  ngrams->clear();
  std::pair<vector<WordID>, size_t> insert_me;
  vector<WordID> &ngram = insert_me.first;
  ngram.reserve(n);
  size_t &id = insert_me.second;
  id = 0;
  for (int refi = 0; refi < references.size(); ++refi) {
    const vector<WordID>& ref = references[refi];
    const int s = ref.size();
    for (int j=0; j<s; ++j) {
      const int remaining = s-j;
      const int k = (n < remaining ? n : remaining);
      ngram.clear();
      for (unsigned int i = 0; i < k; ++i) {
        ngram.push_back(ref[j + i]);
        std::pair<NGramMap::iterator, bool> ret(ngrams->insert(insert_me));
        if (ret.second) {
          counts->resize(id + 1);
          RefCounts &ref_counts = counts->back();
          ref_counts.max = 1;
          ref_counts.refs.resize(references.size());
          ref_counts.refs[refi] = 1;
          ref_counts.length = ngram.size();
          ++id;
        } else {
          RefCounts &ref_counts = (*counts)[ret.first->second];
          ref_counts.max = std::max(ref_counts.max, ++ref_counts.refs[refi]);
        }
      }
    }
  }
}

struct MutableState {
  MutableState(void *from, size_t n) : length(reinterpret_cast<size_t*>(from)), left(reinterpret_cast<WordID *>(length + 1)), right(left + n - 1), counts(reinterpret_cast<GramCount *>(right + n - 1)) {}
  size_t *length;
  WordID *left, *right;
  GramCount *counts;
  static size_t Size(size_t n, size_t bound_ngram_id) { return sizeof(size_t) + (n - 1) * 2 * sizeof(WordID) + bound_ngram_id * sizeof(GramCount); }
};

struct ConstState {
  ConstState(const void *from, size_t n) : length(reinterpret_cast<const size_t*>(from)), left(reinterpret_cast<const WordID *>(length + 1)), right(left + n - 1), counts(reinterpret_cast<const GramCount *>(right + n - 1)) {}
  const size_t *length;
  const WordID *left, *right;
  const GramCount *counts;
  static size_t Size(size_t n, size_t bound_ngram_id) { return sizeof(size_t) + (n - 1) * 2 * sizeof(WordID) + bound_ngram_id * sizeof(GramCount); }
};

template <class T> struct CompatibleHashRange : public std::unary_function<const boost::iterator_range<T> &, size_t> {
  size_t operator()(const boost::iterator_range<T> &range) const {
    return boost::hash_range(range.begin(), range.end());
  }
};

template <class T> struct CompatibleEqualsRange : public std::binary_function<const boost::iterator_range<T> &, const std::vector<WordID> &, size_t> {
  size_t operator()(const boost::iterator_range<T> &range, const std::vector<WordID> &vec) const {
    return boost::algorithm::equals(range, vec);
  }
  size_t operator()(const std::vector<WordID> &vec, const boost::iterator_range<T> &range) const {
    return boost::algorithm::equals(range, vec);
  }
};

void AddWord(const boost::circular_buffer<WordID> &segment, size_t min_length, const NGramMap &ref_grams, GramCount *counters) {
  typedef boost::circular_buffer<WordID>::const_iterator BufferIt;
  typedef boost::iterator_range<BufferIt> SegmentRange;
  if (segment.size() < min_length) return;
#if 0
  CompatibleHashRange<BufferIt> hasher;
  CompatibleEqualsRange<BufferIt> equals;
  for (BufferIt seg_start(segment.end() - min_length); ; --seg_start) {
    NGramMap::const_iterator found = ref_grams.find(SegmentRange(seg_start, segment.end()));
    if (found == ref_grams.end()) break;
    ++counters[found->second];
    if (seg_start == segment.begin()) break;
  }
#endif
}

} // namespace

class TrombleLossComputerImpl {
 public:
  explicit TrombleLossComputerImpl(const std::string &params) : star_(TD::Convert("<{STAR}>")) {
    typedef boost::tokenizer<boost::char_separator<char> > Tokenizer;
    // Argument parsing
    std::string ref_file_name;
    Tokenizer tok(params, boost::char_separator<char>(" "));
    Tokenizer::iterator i = tok.begin();
    if (i == tok.end()) {
      std::cerr << "TrombleLossComputer needs a reference file name." << std::endl;
      exit(1);
    }
    ref_file_name = *i++;
    if (i == tok.end()) {
      std::cerr << "TrombleLossComputer needs to know how many references." << std::endl;
      exit(1);
    }
    num_refs_ = boost::lexical_cast<unsigned int>(*i++);
    for (; i != tok.end(); ++i) {
     thetas_.push_back(boost::lexical_cast<double>(*i));
    }
    if (thetas_.empty()) {
      std::cerr << "TrombleLossComputer is pointless with no weight on n-grams." << std::endl;
      exit(1);
    }

    // Read references file.
    std::ifstream ref_file(ref_file_name.c_str());
    if (!ref_file) {
      std::cerr << "Could not open TrombleLossComputer file " << ref_file_name << std::endl;
      exit(1);
    }
    std::string ref;
    vector<vector<WordID> > references(num_refs_);
    bound_ngram_id_ = 0;
    for (unsigned int sentence = 0; ref_file; ++sentence) {
      for (unsigned int refidx = 0; refidx < num_refs_; ++refidx) {
        if (!getline(ref_file, ref)) {
          if (refidx == 0) break;
          std::cerr << "Short read of " << refidx << " references for sentence " << sentence << std::endl;
          exit(1);
        }
        TD::ConvertSentence(ref, &references[refidx]);
      }
      ref_ids_.resize(sentence + 1);
      ref_counts_.resize(sentence + 1);
      MakeNGramMapFromReferences(references, thetas_.size(), &ref_counts_.back(), &ref_ids_.back());
      bound_ngram_id_ = std::max(bound_ngram_id_, ref_ids_.back().size());
    }
  }

  size_t StateSize() const {
    // n-1 boundary words plus counts for n-grams currently rendered as bytes even though most would fit in bits.
    // Also, this is cached by higher up classes so no need to cache here.
    return MutableState::Size(thetas_.size(), bound_ngram_id_);
  }

  double Traversal(
      const SentenceMetadata &smeta,
      const TRule &rule,
      const vector<const void*> &ant_contexts,
      void *out_context) const {
    // TODO: get refs from sentence metadata.
    // This will require resizable features.
    if (smeta.GetSentenceID() >= ref_ids_.size()) {
      std::cerr << "Sentence ID " << smeta.GetSentenceID() << " doesn't have references; there are only " << ref_ids_.size() << " references." << std::endl;
      exit(1);
    }
    const NGramMap &ngrams = ref_ids_[smeta.GetSentenceID()];
    MutableState out_state(out_context, thetas_.size());
    memset(out_state.counts, 0, bound_ngram_id_ * sizeof(GramCount));
    boost::circular_buffer<WordID> history(thetas_.size());
    std::vector<const void*>::const_iterator ant_context = ant_contexts.begin();
    *out_state.length = 0;
    size_t pushed = 0;
    const size_t keep = thetas_.size() - 1;
    for (vector<WordID>::const_iterator rhs = rule.e().begin(); rhs != rule.e().end(); ++rhs) {
      if (*rhs < 1) {
        assert(ant_context != ant_contexts.end());
        // Constituent
        ConstState rhs_state(*ant_context, thetas_.size());
        *out_state.length += *rhs_state.length;
        {
          GramCount *accum = out_state.counts;
          for (const GramCount *c = rhs_state.counts; c != rhs_state.counts + ngrams.size(); ++c, ++accum) {
            *accum += *c;
          }
        }
        const WordID *w = rhs_state.left;
        bool long_constit = true;
        for (size_t i = 1; i <= keep; ++i, ++w) {
          if (*w == star_) {
            long_constit = false;
            break;
          }
          history.push_back(*w);
          if (++pushed == keep) {
            std::copy(history.begin(), history.end(), out_state.left);
          }
          // Now i is the length of the history coming from this constituent.  So it needs at least i+1 words to have a cross-child add.
          AddWord(history, i + 1, ngrams, out_state.counts);
        }
        // If the consituent is shorter than thetas_.size(), then the
        // constituent's left is the entire constituent, so history is already
        // correct.  Otherwise, the entire right hand side is the entire
        // history.
        if (long_constit) {
          history.assign(thetas_.size(), rhs_state.right, rhs_state.right + keep);
        }
        ++ant_context;
      } else {
        // Word
        ++*out_state.length;
        history.push_back(*rhs);
        if (++pushed == keep) {
          std::copy(history.begin(), history.end(), out_state.left);
        }
        AddWord(history, 1, ngrams, out_state.counts);
      }
    }
    // Fill in left and right constituents.
    if (pushed < keep) {
      std::copy(history.begin(), history.end(), out_state.left);
      for (WordID *i = out_state.left + pushed; i != out_state.left + keep; ++i) {
        *i = star_;
      }
      std::copy(out_state.left, out_state.left + keep, out_state.right);
    } else if(pushed == keep) {
      std::copy(history.begin(), history.end(), out_state.right);
    } else if ((pushed > keep) && !history.empty()) {
      std::copy(history.begin() + 1, history.end(), out_state.right);
    }
    std::vector<RefCounts>::const_iterator ref_info = ref_counts_[smeta.GetSentenceID()].begin();
    // Clip the counts and count matches.
    // Indexed by reference then by length.
    std::vector<std::vector<unsigned int> > matches(num_refs_, std::vector<unsigned int>(thetas_.size()));
    for (GramCount *c = out_state.counts; c != out_state.counts + ngrams.size(); ++c, ++ref_info) {
      *c = std::min(*c, ref_info->max);
      if (*c) {
        for (unsigned int refidx = 0; refidx < num_refs_; ++refidx) {
          assert(ref_info->length >= 1);
          assert(ref_info->length - 1 < thetas_.size());
          matches[refidx][ref_info->length - 1] += std::min(*c, ref_info->refs[refidx]);
        }
      }
    }
    double best_score = 0.0;
    for (unsigned int refidx = 0; refidx < num_refs_; ++refidx) {
      double score = 0.0;
      for (unsigned int j = 0; j < std::min(*out_state.length, thetas_.size()); ++j) {
        score += thetas_[j] * static_cast<double>(matches[refidx][j]) / static_cast<double>(*out_state.length - j);
      }
      best_score = std::max(best_score, score);
    }
    return best_score;
  }

 private:
  unsigned int num_refs_;
  // Indexed by sentence id.
  std::vector<NGramMap> ref_ids_;
  // Then by id from ref_ids_.
  std::vector<std::vector<RefCounts> > ref_counts_;

  // thetas_[0] is the weight for 1-grams
  std::vector<double> thetas_;

  // All ngram ids in ref_ids_ are < this value.
  size_t bound_ngram_id_;

  const WordID star_;
};

TrombleLossComputer::TrombleLossComputer(const std::string &params) :
    boost::base_from_member<PImpl>(new TrombleLossComputerImpl(params)),
    FeatureFunction(boost::base_from_member<PImpl>::member->StateSize()),
    fid_(FD::Convert("TrombleLossComputer")) {}

TrombleLossComputer::~TrombleLossComputer() {}

void TrombleLossComputer::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* out_context) const {
  (void) estimated_features;
  const double loss = boost::base_from_member<PImpl>::member->Traversal(smeta, *edge.rule_, ant_contexts, out_context);
  features->set_value(fid_, loss);
}
