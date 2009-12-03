#include "viterbi.h"

#include <vector>
#include "hg.h"

using namespace std;

string ViterbiETree(const Hypergraph& hg) {
  vector<WordID> tmp;
  const prob_t p = Viterbi<vector<WordID>, ETreeTraversal, prob_t, EdgeProb>(hg, &tmp);
  return TD::GetString(tmp);
}

string ViterbiFTree(const Hypergraph& hg) {
  vector<WordID> tmp;
  const prob_t p = Viterbi<vector<WordID>, FTreeTraversal, prob_t, EdgeProb>(hg, &tmp);
  return TD::GetString(tmp);
}

prob_t ViterbiESentence(const Hypergraph& hg, vector<WordID>* result) {
  return Viterbi<vector<WordID>, ESentenceTraversal, prob_t, EdgeProb>(hg, result);
}

prob_t ViterbiFSentence(const Hypergraph& hg, vector<WordID>* result) {
  return Viterbi<vector<WordID>, FSentenceTraversal, prob_t, EdgeProb>(hg, result);
}

int ViterbiELength(const Hypergraph& hg) {
  int len = -1;
  Viterbi<int, ELengthTraversal, prob_t, EdgeProb>(hg, &len);
  return len;
}

int ViterbiPathLength(const Hypergraph& hg) {
  int len = -1;
  Viterbi<int, PathLengthTraversal, prob_t, EdgeProb>(hg, &len);
  return len;
}

