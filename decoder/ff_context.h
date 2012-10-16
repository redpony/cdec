
#ifndef _FF_CONTEXT_H_
#define _FF_CONTEXT_H_

#include <vector>
#include <boost/xpressive/xpressive.hpp>
#include "ff.h"

using namespace boost::xpressive;
using namespace std;

class RuleContextFeatures : public FeatureFunction {
 public:
  RuleContextFeatures(const string& param);
 protected:
  virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const HG::Edge& edge,
                                     const vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
  virtual void PrepareForInput(const SentenceMetadata& smeta);
  virtual void ParseArgs(const string& in);
  virtual string Escape(const string& x) const;
  virtual void ReplaceMacroWithString(string& feature_instance, 
				      bool token_vs_label, 
				      int relative_location, 
				      const string& actual_token) const;
  virtual void ReplaceTokenMacroWithString(string& feature_instance, 
					   int relative_location, 
					   const string& actual_token) const;
  virtual void ReplaceLabelMacroWithString(string& feature_instance, 
					   int relative_location, 
					   const string& actual_token) const;
  virtual void Error(const string&) const;

 private:
  vector<int> token_relative_locations, label_relative_locations;
  string feature_template;
  vector<WordID> current_input;
  WordID kSOS, kEOS;
  sregex macro_regex;
  
};

#endif
