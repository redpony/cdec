#include "ff_context.h"

#include <stdlib.h>
#include <sstream>
#include <cassert>
#include <cmath>

#include "hg.h"
#include "filelib.h"
#include "stringlib.h"
#include "sentence_metadata.h"
#include "lattice.h"
#include "fdict.h"
#include "verbose.h"
#include "tdict.h"

RuleContextFeatures::RuleContextFeatures(const string& param) {
  //  cerr << "initializing RuleContextFeatures with parameters: " << param;
  kSOS = TD::Convert("<s>");
  kEOS = TD::Convert("</s>");
  macro_regex = sregex::compile("%([xy])\\[(-[1-9][0-9]*|0|[1-9][1-9]*)]");
  ParseArgs(param);
}

string RuleContextFeatures::Escape(const string& x) const {
  string y = x;
  for (int i = 0; i < y.size(); ++i) {
    if (y[i] == '=') y[i]='_';
    if (y[i] == ';') y[i]='_';
  }
  return y;
}

// replace %x[relative_location] or %y[relative_location] with actual_token
// within feature_instance
void RuleContextFeatures::ReplaceMacroWithString(
  string& feature_instance, bool token_vs_label, int relative_location, 
  const string& actual_token) const {

  stringstream macro;
  if (token_vs_label) {
    macro << "%x[";
  } else {
    macro << "%y[";
  }
  macro << relative_location << "]";
  int macro_index = feature_instance.find(macro.str());
  if (macro_index == string::npos) {
    cerr << "Can't find macro " << macro.str() << " in feature template " 
	 << feature_instance;
    abort();
  }
  feature_instance.replace(macro_index, macro.str().size(), actual_token);
}

void RuleContextFeatures::ReplaceTokenMacroWithString(
  string& feature_instance, int relative_location, 
  const string& actual_token) const {

  ReplaceMacroWithString(feature_instance, true, relative_location, 
				actual_token);
}

void RuleContextFeatures::ReplaceLabelMacroWithString(
  string& feature_instance, int relative_location, 
  const string& actual_token) const {

  ReplaceMacroWithString(feature_instance, false, relative_location, 
				actual_token);
}

void RuleContextFeatures::Error(const string& error_message) const {
  cerr << "Error: " << error_message << "\n\n"

       << "RuleContextFeatures Usage: \n"			     
       << "  feature_function=RuleContextFeatures -t <TEMPLATE>\n\n" 

       << "Example <TEMPLATE>: U1:%x[-1]_%x[0]|%y[0]\n\n" 

       << "%x[k] and %y[k] are macros to be instantiated with an input\n" 
       << "token (for x) or a label (for y). k specifies the relative\n" 
       << "location of the input token or label with respect to the current\n"
       << "position. For x, k is an integer value. For y, k must be 0 (to\n" 
       << "be extended).\n\n";

  abort();
}

void RuleContextFeatures::ParseArgs(const string& in) {
  vector<string> const& argv = SplitOnWhitespace(in);
  for (vector<string>::const_iterator i = argv.begin(); i != argv.end(); ++i) {
    string const& s = *i;
    if (s[0] == '-') {
      if (s.size() > 2) { 
	stringstream msg;
	msg << s << " is an invalid option for RuleContextFeatures.";
	Error(msg.str());
      }

      switch (s[1]) {

      // feature template
      case 't': {
	if (++i == argv.end()) { 
	  Error("Can't find template."); 
	}
	feature_template = *i;
	string::const_iterator start = feature_template.begin();
	string::const_iterator end = feature_template.end();
	smatch macro_match;

        // parse the template
	while (regex_search(start, end, macro_match, macro_regex)) {
	  // get the relative location
	  string relative_location_str(macro_match[2].first, 
				       macro_match[2].second);
	  int relative_location = atoi(relative_location_str.c_str());
	  // add it to the list of relative locations for token or label
	  // (i.e. x or y)
	  bool valid_location = true;
          if (*macro_match[1].first == 'x') {
	    // add it to token locations
	    token_relative_locations.push_back(relative_location);
	  } else {
	    if (relative_location != 0) { valid_location = false; }
	    // add it to label locations
	    label_relative_locations.push_back(relative_location);
	  }
	  if (!valid_location) {
	    stringstream msg;
	    msg << "Relative location " << relative_location
		<< " in feature template " << feature_template
		<< " is invalid.";
	    Error(msg.str());
	  }
	  start = macro_match[0].second;
	}
	break;
      }

      // TODO: arguments to specify kSOS and kEOS

      default: {
	stringstream msg;
	msg << "Invalid option on RuleContextFeatures: " << s;
	Error(msg.str());
	break;
      }
      } // end of switch
    } // end of if (token starts with hyphen)
  } // end of for loop (over arguments)

  // the -t (i.e. template) option is mandatory in this feature function
  if (label_relative_locations.size() == 0 || 
      token_relative_locations.size() == 0) {
    stringstream msg;
    msg << "Feature template must specify at least one" 
	 << "token macro (e.g. x[-1]) and one label macro (e.g. y[0]).";
    Error(msg.str());
  }
}

void RuleContextFeatures::PrepareForInput(const SentenceMetadata& smeta) {
  const Lattice& sl = smeta.GetSourceLattice();
  current_input.resize(sl.size());
  for (unsigned i = 0; i < sl.size(); ++i) {
    if (sl[i].size() != 1) {
      stringstream msg;
      msg << "RuleContextFeatures don't support lattice inputs!\nid=" 
	   << smeta.GetSentenceId() << endl;
      Error(msg.str());
    }
    current_input[i] = sl[i][0].label;
  }
}

void RuleContextFeatures::TraversalFeaturesImpl(
  const SentenceMetadata& smeta, const Hypergraph::Edge& edge,
  const vector<const void*>& ant_contexts, SparseVector<double>* features,
  SparseVector<double>* estimated_features, void* context) const {

  const TRule& rule = *edge.rule_;
  // arity = 0, no nonterminals
  // size = 1, predicted label is a single token
  if (rule.Arity() != 0 ||
      rule.e_.size() != 1) { 
    return; 
  }

  // replace label macros with actual label strings
  // NOTE: currently, this feature function doesn't allow any label
  // macros except %y[0]. but you can look at as much of the source as you want
  const WordID y0 = rule.e_[0];
  string y0_str = TD::Convert(y0);

  // start of the span in the input being labeled
  const int from_src_index = edge.i_;   
  // end of the span in the input
  const int to_src_index = edge.j_;

  // in the case of tagging the size of the spans being labeled will
  //  always be 1, but in other formalisms, you can have bigger spans
  if (to_src_index - from_src_index != 1) {
    cerr << "RuleContextFeatures doesn't support input spans of length != 1";
    abort();
  }

  string feature_instance = feature_template;
  // replace token macros with actual token strings
  for (unsigned i = 0; i < token_relative_locations.size(); ++i) {
    int loc = token_relative_locations[i];
    WordID x = loc < 0? kSOS: kEOS;
    if(from_src_index + loc >= 0 && 
       from_src_index + loc < current_input.size()) {
      x = current_input[from_src_index + loc];
    }
    string x_str = TD::Convert(x);
    ReplaceTokenMacroWithString(feature_instance, loc, x_str);
  }
    
  ReplaceLabelMacroWithString(feature_instance, 0, y0_str);

  // pick a real value for this feature
  double fval = 1.0; 

  // add it to the feature vector
  //   FD::Convert converts the feature string to a feature int
  //   Escape makes sure the feature string doesn't have any bad
  //     symbols that could confuse a parser somewhere
  features->add_value(FD::Convert(Escape(feature_instance)), fval);
}
