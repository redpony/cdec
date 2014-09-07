#ifndef FF_LEXICAL_H_
#define FF_LEXICAL_H_

#include <vector>
#include <map>
#include "trule.h"
#include "ff.h"
#include "hg.h"
#include "array2d.h"
#include "wordid.h"
#include <sstream>
#include <cassert>
#include <cmath>

#include "filelib.h"
#include "stringlib.h"
#include "sentence_metadata.h"
#include "lattice.h"
#include "fdict.h"
#include "verbose.h"
#include "tdict.h"
#include "hg.h"

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

class LexicalFeatures : public FeatureFunction {
public:
	LexicalFeatures(const std::string& param) {
		if (param.empty()) {
			cerr << "LexicalFeatures: using T,D,I\n";
			T_ = true; I_ = true; D_ = true;
		} else {
			const vector<string> argv = SplitOnWhitespace(param);
			assert(argv.size() == 3);
			T_ = (bool) atoi(argv[0].c_str());
			I_ = (bool) atoi(argv[1].c_str());
			D_ = (bool) atoi(argv[2].c_str());
			cerr << "T=" << T_ << " I=" << I_ << " D=" << D_ << endl;
		}
	};
	static std::string usage(bool p,bool d) {
	    return usage_helper("LexicalFeatures","[0/1 0/1 0/1]","Sparse lexical word translation indicator features. If arguments are supplied, specify like this: translations insertions deletions",p,d);
	}
protected:
	virtual void TraversalFeaturesImpl(const SentenceMetadata& smeta,
			const HG::Edge& edge,
			const std::vector<const void*>& ant_contexts,
			SparseVector<double>* features,
			SparseVector<double>* estimated_features,
			void* context) const;
	virtual void PrepareForInput(const SentenceMetadata& smeta);
private:
	mutable std::map<const TRule*, SparseVector<double> > rule2feats_;
	bool T_;
	bool I_;
	bool D_;
};

void LexicalFeatures::PrepareForInput(const SentenceMetadata& smeta) {
  rule2feats_.clear(); //  std::map<const TRule*, SparseVector<double> >
}

void LexicalFeatures::TraversalFeaturesImpl(const SentenceMetadata& smeta,
	const HG::Edge& edge,
	const std::vector<const void*>& ant_contexts,
	SparseVector<double>* features,
	SparseVector<double>* estimated_features,
	void* context) const {
	
	map<const TRule*, SparseVector<double> >::iterator it = rule2feats_.find(edge.rule_.get());	
	if (it == rule2feats_.end()) {
		const TRule& rule = *edge.rule_;
	    it = rule2feats_.insert(make_pair(&rule, SparseVector<double>())).first;
	    SparseVector<double>& f = it->second;
	    std::vector<bool> sf(edge.rule_->FLength(),false); // stores if source tokens are visited by alignment points
		std::vector<bool> se(edge.rule_->ELength(),false); // stores if target tokens are visited by alignment points
		int fid = 0;
	    // translations
	    for (unsigned i=0;i<rule.a_.size();++i) {
	    	const AlignmentPoint& ap = rule.a_[i];
	    	sf[ap.s_] = true; // mark index as seen
	    	se[ap.t_] = true; // mark index as seen
	    	ostringstream os;
			os << "LT:" << Escape(TD::Convert(rule.f_[ap.s_])) << ":" << Escape(TD::Convert(rule.e_[ap.t_]));
			fid = FD::Convert(os.str());
			if (fid <= 0) continue;
			if (T_)
				f.add_value(fid, 1.0);
	    }
	    // word deletions
	    for (unsigned i=0;i<sf.size();++i) {
	    	if (!sf[i] && rule.f_[i] > 0) {// if not visited and is terminal
	    		ostringstream os;
	    		os << "LD:" << Escape(TD::Convert(rule.f_[i]));
	    		fid = FD::Convert(os.str());
	    		if (fid <= 0) continue;
	    		if (D_)
		    		f.add_value(fid, 1.0);
	    	}
	    }
	    // word insertions
	    for (unsigned i=0;i<se.size();++i) {
	    	if (!se[i] && rule.e_[i] >= 1) {// if not visited and is terminal
	    		ostringstream os;
	    		os << "LI:" << Escape(TD::Convert(rule.e_[i]));
	    		fid = FD::Convert(os.str());
	    		if (fid <= 0) continue;
	    		if (I_)
		    		f.add_value(fid, 1.0);
	    	}
	    }
	}
	(*features) += it->second;
}


#endif
