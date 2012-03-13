#include "ff_context.h"

#include <sstream>
#include <cassert>
#include <cmath>

#include "filelib.h"
#include "stringlib.h"
#include "sentence_metadata.h"
#include "lattice.h"
#include "fdict.h"
#include "verbose.h"

using namespace std;

namespace {
  string Escape(const string& x) {
    string y = x;
    for (int i = 0; i < y.size(); ++i) {
      if (y[i] == '=') y[i]='_';
      if (y[i] == ';') y[i]='_';
    }
    return y;
  }
}

RuleContextFeatures::RuleContextFeatures(const std::string& param) {
  kSOS = TD::Convert("<s>");
  kEOS = TD::Convert("</s>");

  // TODO param lets you pass in a string from the cdec.ini file
}

void RuleContextFeatures::PrepareForInput(const SentenceMetadata& smeta) {
  const Lattice& sl = smeta.GetSourceLattice();
  current_input.resize(sl.size());
  for (unsigned i = 0; i < sl.size(); ++i) {
    if (sl[i].size() != 1) {
      cerr << "Context features not supported with lattice inputs!\nid=" << smeta.GetSentenceId() << endl;
      abort();
    }
    current_input[i] = sl[i][0].label;
  }
}

void RuleContextFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                                const Hypergraph::Edge& edge,
                                                const vector<const void*>& ant_contexts,
                                                SparseVector<double>* features,
                                                SparseVector<double>* estimated_features,
                                                void* context) const {
  const TRule& rule = *edge.rule_;

  if (rule.Arity() != 0 || // arity = 0, no nonterminals
      rule.e_.size() != 1) return; // size = 1, predicted label is a single token


  // you can see the current label "for free"
  const WordID cur_label = rule.e_[0];
  // (if you want to see more labels, you have to be very careful, and muck
  //  about with contexts and ant_contexts)

  // but... you can look at as much of the source as you want!
  const int from_src_index = edge.i_;   // start of the span in the input being labeled
  const int to_src_index = edge.j_;     // end of the span in the input
  // (note: in the case of tagging the size of the spans being labeled will
  //  always be 1, but in other formalisms, you can have bigger spans.)

  // this is the current token being labeled:
  const WordID cur_input = current_input[from_src_index];

  // let's get the previous token in the input (may be to the left of the start
  // of the sentence!)
  WordID prev_input = kSOS;
  if (from_src_index > 0) { prev_input = current_input[from_src_index - 1]; }
  // let's get the next token (may be to the left of the start of the sentence!)
  WordID next_input = kEOS;
  if (to_src_index < current_input.size()) { next_input = current_input[to_src_index]; }

  // now, build a feature string
  ostringstream os;
  // TD::Convert converts from the internal integer representation of a token
  // to the actual token
  os << "C1:" << TD::Convert(prev_input) << '_' 
     << TD::Convert(cur_input) << '|' << TD::Convert(cur_label);
  // C1 is just to prevent a name clash

  // pick a value
  double fval = 1.0; // can be any real value

  // add it to the feature vector FD::Convert converts the feature string to a
  // feature int, Escape makes sure the feature string doesn't have any bad
  // symbols that could confuse a parser somewhere
  features->add_value(FD::Convert(Escape(os.str())), fval);
  // that's it!

  // create more features if you like...
}

