#include "viterbi.h"

#include <sstream>
#include <vector>
#include "hg.h"

using namespace std;

std::string viterbi_stats(Hypergraph const& hg, std::string const& name, bool estring, bool etree)
{
  ostringstream o;
  o << hg.stats(name);
  if (estring) {
    vector<WordID> trans;
    const prob_t vs = ViterbiESentence(hg, &trans);
    o<<name<<"       Viterbi: "<<log(vs)<<endl;
    o<<name<<"       Viterbi: "<<TD::GetString(trans)<<endl;
  }
  if (etree) {
    o<<name<<"          tree: "<<ViterbiETree(hg)<<endl;
  }
  return o.str();
}


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

// create a strings of the form (S (X the man) (X said (X he (X would (X go)))))
struct JoshuaVisTraversal {
  JoshuaVisTraversal() : left("("), space(" "), right(")") {}
  const std::string left;
  const std::string space;
  const std::string right;
  void operator()(const Hypergraph::Edge& edge,
                  const std::vector<const std::vector<WordID>*>& ants,
                  std::vector<WordID>* result) const {
    std::vector<WordID> tmp;
    edge.rule_->ESubstitute(ants, &tmp);
    const std::string cat = TD::Convert(edge.rule_->GetLHS() * -1);
    if (cat == "Goal")
      result->swap(tmp);
    else {
      ostringstream os;
      os << left << cat << '{' << edge.i_ << '-' << edge.j_ << '}'
         << space << TD::GetString(tmp) << right;
      TD::ConvertSentence(os.str(),
                          result);
    }
  }
};

string JoshuaVisualizationString(const Hypergraph& hg) {
  vector<WordID> tmp;
  const prob_t p = Viterbi<vector<WordID>, JoshuaVisTraversal, prob_t, EdgeProb>(hg, &tmp);
  return TD::GetString(tmp);
}

