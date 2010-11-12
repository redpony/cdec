#include "ff_wordset.h"

#include "fdict.h"
#include <sstream>
#include <iostream>

using namespace std;

void WordSet::TraversalFeaturesImpl(const SentenceMetadata& /*smeta*/ ,
				    const Hypergraph::Edge& edge,
				    const vector<const void*>& /* ant_contexts */,
				    SparseVector<double>* features,
				    SparseVector<double>* /* estimated_features */,
				    void* /* context */) const {

  double addScore = 0.0;
  for(std::vector<WordID>::const_iterator it = edge.rule_->e_.begin(); it != edge.rule_->e_.end(); ++it) {
    
    bool inVocab = (vocab_.find(*it) != vocab_.end());
    if(oovMode_ && !inVocab) {
      addScore += 1.0;
    } else if(!oovMode_ && inVocab) {
      addScore += 1.0;
    }
  }
  features->set_value(fid_, addScore);
}

