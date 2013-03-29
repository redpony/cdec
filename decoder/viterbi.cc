#include "fast_lexical_cast.hpp"
#include "viterbi.h"

#include <sstream>
#include <vector>
#include "hg.h"


//#define DEBUG_VITERBI_SORT

using namespace std;

std::string viterbi_stats(Hypergraph const& hg, std::string const& name, bool estring, bool etree,bool show_derivation, bool extract_rules, boost::shared_ptr<WriteFile> extract_file)
{
  ostringstream o;
  o << hg.stats(name);
  if (estring) {
    vector<WordID> trans;
    const prob_t vs = ViterbiESentence(hg, &trans);
    o<<name<<"  Viterbi logp: "<<log(vs)<< endl;
    o<<name<<"       Viterbi: "<<TD::GetString(trans)<<endl;
  }
  if (etree) {
    o<<name<<"          tree: "<<ViterbiETree(hg)<<endl;
  }
  if (extract_rules) {
    ViterbiRules(hg, extract_file->stream());
  }
  if (show_derivation) {
    o<<name<<"          derivation: ";
    o << hg.show_viterbi_tree(false); // last item should be goal (or at least depend on prev items).  TODO: this doesn't actually reorder the nodes in hg.
    o<<endl;
  }
#ifdef DEBUG_VITERBI_SORT
  const_cast<Hypergraph&>(hg).ViterbiSortInEdges();
  o<<name<<"sorted #1 derivation: ";
  o<<hg.show_first_tree(false);
  o<<endl;
#endif
  return o.str();
}

void ViterbiRules(const Hypergraph& hg, ostream* o) {
   vector<Hypergraph::Edge const*> edges;
   Viterbi<ViterbiPathTraversal>(hg, &edges);
   for (unsigned i = 0; i < edges.size(); i++)
      (*o) << edges[i]->rule_->AsString(true) << endl;
}

string ViterbiETree(const Hypergraph& hg) {
  vector<WordID> tmp;
  Viterbi<ETreeTraversal>(hg, &tmp);
  return TD::GetString(tmp);
}

string ViterbiFTree(const Hypergraph& hg) {
  vector<WordID> tmp;
  Viterbi<FTreeTraversal>(hg, &tmp);
  return TD::GetString(tmp);
}

prob_t ViterbiESentence(const Hypergraph& hg, vector<WordID>* result) {
  return Viterbi<ESentenceTraversal>(hg, result);
}

prob_t ViterbiFSentence(const Hypergraph& hg, vector<WordID>* result) {
  return Viterbi<FSentenceTraversal>(hg, result);
}

int ViterbiELength(const Hypergraph& hg) {
  int len = -1;
  Viterbi<ELengthTraversal>(hg, &len);
  return len;
}

int ViterbiPathLength(const Hypergraph& hg) {
  int len = -1;
  Viterbi<PathLengthTraversal>(hg, &len);
  return len;
}

// create a strings of the form (S (X the man) (X said (X he (X would (X go)))))
struct JoshuaVisTraversal {
  JoshuaVisTraversal() : left("("), space(" "), right(")") {}
  const std::string left;
  const std::string space;
  const std::string right;
  typedef std::vector<WordID> Result;
  void operator()(const Hypergraph::Edge& edge,
                  const std::vector<const Result*>& ants,
                  Result* result) const {
    Result tmp;
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
  Viterbi<JoshuaVisTraversal>(hg, &tmp);
  return TD::GetString(tmp);
}


//TODO: move to appropriate header if useful elsewhere
/*
  The simple solution like abs(f1-f2) <= e does not work for very small or very big values. This floating-point comparison algorithm is based on the more confident solution presented by Knuth in [1]. For a given floating point values u and v and a tolerance e:

| u - v | <= e * |u| and | u - v | <= e * |v|
defines a "very close with tolerance e" relationship between u and v
        (1)

| u - v | <= e * |u| or   | u - v | <= e * |v|
defines a "close enough with tolerance e" relationship between u and v
        (2)

Both relationships are commutative but are not transitive. The relationship defined by inequations (1) is stronger that the relationship defined by inequations (2) (i.e. (1) => (2) ). Because of the multiplication in the right side of inequations, that could cause an unwanted underflow condition, the implementation is using modified version of the inequations (1) and (2) where all underflow, overflow conditions could be guarded safely:

| u - v | / |u| <= e and | u - v | / |v| <= e
| u - v | / |u| <= e or   | u - v | / |v| <= e
        (1`)
(2`)
*/
#include <cmath>
#include <stdexcept>
inline bool close_enough(double a,double b,double epsilon)
{
    using std::fabs;
    double diff=fabs(a-b);
    return diff<=epsilon*fabs(a) || diff<=epsilon*fabs(b);
}

SparseVector<double> ViterbiFeatures(Hypergraph const& hg,WeightVector const* weights,bool fatal_dotprod_disagreement) {
  SparseVector<double> r;
  const prob_t p = Viterbi<FeatureVectorTraversal>(hg, &r);
  if (weights) {
    double logp=log(p);
    double fv=r.dot(*weights);
    const double EPSILON=1e-5;
    if (!close_enough(logp,fv,EPSILON)) {
      string complaint="ViterbiFeatures log prob disagrees with features.dot(weights)"+boost::lexical_cast<string>(logp)+"!="+boost::lexical_cast<string>(fv);
      if (fatal_dotprod_disagreement)
        throw std::runtime_error(complaint);
      else
        cerr<<complaint<<endl;
    }
  }
  return r;
}

