#ifndef _FAST_INTERSECTOR_H_
#define _FAST_INTERSECTOR_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include <boost/functional/hash.hpp>

using namespace std;

typedef boost::hash<vector<int> > VectorHash;
typedef unordered_map<vector<int>, vector<int>, VectorHash> Index;

class Phrase;
class PhraseLocation;
class Precomputation;
class SuffixArray;
class Vocabulary;

class FastIntersector {
 public:
  FastIntersector(shared_ptr<SuffixArray> suffix_array,
                  shared_ptr<Precomputation> precomputation,
                  shared_ptr<Vocabulary> vocabulary,
                  int max_rule_span,
                  int min_gap_size);

  virtual ~FastIntersector();

  virtual PhraseLocation Intersect(PhraseLocation& prefix_location,
                                   PhraseLocation& suffix_location,
                                   const Phrase& phrase);

 protected:
  FastIntersector();

 private:
  vector<int> ConvertPhrase(const vector<int>& old_phrase);

  int EstimateNumOperations(const PhraseLocation& phrase_location,
                            bool has_margin_x) const;

  PhraseLocation ExtendPrefixPhraseLocation(PhraseLocation& prefix_location,
                                            const Phrase& phrase,
                                            bool prefix_ends_with_x,
                                            int next_symbol) const;

  PhraseLocation ExtendSuffixPhraseLocation(PhraseLocation& suffix_location,
                                            const Phrase& phrase,
                                            bool suffix_starts_with_x,
                                            int prev_symbol) const;

  void ExtendPhraseLocation(PhraseLocation& location) const;

  pair<int, int> GetSearchRange(bool has_marginal_x) const;

  shared_ptr<SuffixArray> suffix_array;
  shared_ptr<Vocabulary> vocabulary;
  int max_rule_span;
  int min_gap_size;
  Index collocations;
};

#endif
