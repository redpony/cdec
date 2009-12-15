#include "ff_wordalign.h"

#include <string>
#include <cmath>

#include "stringlib.h"
#include "sentence_metadata.h"
#include "hg.h"
#include "fdict.h"
#include "aligner.h"
#include "tdict.h"   // Blunsom hack
#include "filelib.h" // Blunsom hack

using namespace std;

RelativeSentencePosition::RelativeSentencePosition(const string& param) :
    fid_(FD::Convert("RelativeSentencePosition")) {
  if (!param.empty()) {
    cerr << "  Loading word classes from " << param << endl;
    condition_on_fclass_ = true;
    template_ = "RSP:FC000";
    assert(!"not implemented");
  } else {
    condition_on_fclass_ = false;
  }
}

void RelativeSentencePosition::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                                     const Hypergraph::Edge& edge,
                                                     const vector<const void*>& ant_states,
                                                     SparseVector<double>* features,
                                                     SparseVector<double>* estimated_features,
                                                     void* state) const {
  // if the source word is either null or the generated word
  // has no position in the reference
  if (edge.i_ == -1 || edge.prev_i_ == -1)
    return;

  assert(smeta.GetTargetLength() > 0);
  const double val = fabs(static_cast<double>(edge.i_) / smeta.GetSourceLength() -
                          static_cast<double>(edge.prev_i_) / smeta.GetTargetLength());
  features->set_value(fid_, val);
  if (condition_on_fclass_) {
    assert(!"not implemented");
  }
//  cerr << f_len_ << " " << e_len_ << " [" << edge.i_ << "," << edge.j_ << "|" << edge.prev_i_ << "," << edge.prev_j_ << "]\t" << edge.rule_->AsString() << "\tVAL=" << val << endl;
}

MarkovJump::MarkovJump(const string& param) :
    FeatureFunction(1),
    fid_(FD::Convert("MarkovJump")),
    individual_params_per_jumpsize_(false),
    condition_on_flen_(false) {
  cerr << "    MarkovJump";
  vector<string> argv;
  int argc = SplitOnWhitespace(param, &argv);
  if (argc > 0) {
    if (argv[0] == "--fclasses") {
      argc--;
      assert(argc > 0);
      const string f_class_file = argv[1];
    }
    if (argc != 1 || !(argv[0] == "-f" || argv[0] == "-i" || argv[0] == "-if")) {
      cerr << "MarkovJump: expected parameters to be -f, -i, or -if\n";
      exit(1);
    }
    individual_params_per_jumpsize_ = (argv[0][1] == 'i');
    condition_on_flen_ = (argv[0][argv[0].size() - 1] == 'f');
    if (individual_params_per_jumpsize_) {
      template_ = "Jump:000";
      cerr << ", individual jump parameters";
      if (condition_on_flen_) {
        template_ += ":F00";
        cerr << " (split by f-length)";
      }
    }
  } else {
    cerr << " (Blunsom & Cohn definition)";
  }
  cerr << endl;
}

void MarkovJump::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                       const Hypergraph::Edge& edge,
                                       const vector<const void*>& ant_states,
                                       SparseVector<double>* features,
                                       SparseVector<double>* estimated_features,
                                       void* state) const {
  unsigned char& dpstate = *((unsigned char*)state);
  if (edge.Arity() == 0) {
    dpstate = static_cast<unsigned int>(edge.i_);
  } else if (edge.Arity() == 1) {
    dpstate = *((unsigned char*)ant_states[0]);
  } else if (edge.Arity() == 2) {
    int left_index = *((unsigned char*)ant_states[0]);
    int right_index = *((unsigned char*)ant_states[1]);
    if (right_index == -1)
      dpstate = static_cast<unsigned int>(left_index);
    else
      dpstate = static_cast<unsigned int>(right_index);
    const int jumpsize = right_index - left_index;
    features->set_value(fid_, fabs(jumpsize - 1));  // Blunsom and Cohn def

    if (individual_params_per_jumpsize_) {
      string fname = template_;
      int param = jumpsize;
      if (jumpsize < 0) {
        param *= -1;
        fname[5]='L';
      } else if (jumpsize > 0) {
        fname[5]='R';
      }
      if (param) {
        fname[6] = '0' + (param / 10);
        fname[7] = '0' + (param % 10);
      }
      if (condition_on_flen_) {
        const int flen = smeta.GetSourceLength();
        fname[10] = '0' + (flen / 10);
        fname[11] = '0' + (flen % 10);
      }
      features->set_value(FD::Convert(fname), 1.0);
    }
  } else {
    assert(!"something really unexpected is happening");
  }
}

AlignerResults::AlignerResults(const std::string& param) :
    cur_sent_(-1),
    cur_grid_(NULL) {
  vector<string> argv;
  int argc = SplitOnWhitespace(param, &argv);
  if (argc != 2) {
    cerr << "Required format: AlignerResults [FeatureName] [file.pharaoh]\n";
    exit(1);
  }
  cerr << "  feature: " << argv[0] << "\talignments: " << argv[1] << endl;
  fid_ = FD::Convert(argv[0]);
  ReadFile rf(argv[1]);
  istream& in = *rf.stream(); int lc = 0;
  while(in) {
    string line;
    getline(in, line);
    if (!in) break; 
    ++lc;
    is_aligned_.push_back(AlignerTools::ReadPharaohAlignmentGrid(line));
  }
  cerr << "  Loaded " << lc << " refs\n";
}

void AlignerResults::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                           const Hypergraph::Edge& edge,
                                           const vector<const void*>& ant_states,
                                           SparseVector<double>* features,
                                           SparseVector<double>* estimated_features,
                                           void* state) const {
  if (edge.i_ == -1 || edge.prev_i_ == -1)
    return;

  if (cur_sent_ != smeta.GetSentenceID()) {
    assert(smeta.HasReference());
    cur_sent_ = smeta.GetSentenceID();
    assert(cur_sent_ < is_aligned_.size());
    cur_grid_ = is_aligned_[cur_sent_].get();
  }

  //cerr << edge.rule_->AsString() << endl;

  int j = edge.i_;        // source side (f)
  int i = edge.prev_i_;   // target side (e)
  if (j < cur_grid_->height() && i < cur_grid_->width() && (*cur_grid_)(i, j)) {
//    if (edge.rule_->e_[0] == smeta.GetReference()[i][0].label) {
      features->set_value(fid_, 1.0);
//      cerr << edge.rule_->AsString() << "   (" << i << "," << j << ")\n";
//    }
  }
}

BlunsomSynchronousParseHack::BlunsomSynchronousParseHack(const string& param) :
  FeatureFunction((100 / 8) + 1), fid_(FD::Convert("NotRef")), cur_sent_(-1) {
  ReadFile rf(param);
  istream& in = *rf.stream(); int lc = 0;
  while(in) {
    string line;
    getline(in, line);
    if (!in) break; 
    ++lc;
    refs_.push_back(vector<WordID>());
    TD::ConvertSentence(line, &refs_.back());
  }
  cerr << "  Loaded " << lc << " refs\n";
}

void BlunsomSynchronousParseHack::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                           const Hypergraph::Edge& edge,
                                           const vector<const void*>& ant_states,
                                           SparseVector<double>* features,
                                           SparseVector<double>* estimated_features,
                                           void* state) const {
  if (cur_sent_ != smeta.GetSentenceID()) {
    // assert(smeta.HasReference());
    cur_sent_ = smeta.GetSentenceID();
    assert(cur_sent_ < refs_.size());
    cur_ref_ = &refs_[cur_sent_];
    cur_map_.clear();
    for (int i = 0; i < cur_ref_->size(); ++i) {
      vector<WordID> phrase;
      for (int j = i; j < cur_ref_->size(); ++j) {
        phrase.push_back((*cur_ref_)[j]);
        cur_map_[phrase] = i;
      }
    }
  }
  //cerr << edge.rule_->AsString() << endl;
  for (int i = 0; i < ant_states.size(); ++i) {
    if (DoesNotBelong(ant_states[i])) {
      //cerr << "  ant " << i << " does not belong\n";
      return;
    }
  }
  vector<vector<WordID> > ants(ant_states.size());
  vector<const vector<WordID>* > pants(ant_states.size());
  for (int i = 0; i < ant_states.size(); ++i) {
    AppendAntecedentString(ant_states[i], &ants[i]);
    //cerr << "  ant[" << i << "]: " << ((int)*(static_cast<const unsigned char*>(ant_states[i]))) << " " << TD::GetString(ants[i]) << endl;
    pants[i] = &ants[i];
  }
  vector<WordID> yield;
  edge.rule_->ESubstitute(pants, &yield);
  //cerr << "YIELD: " << TD::GetString(yield) << endl;
  Vec2Int::iterator it = cur_map_.find(yield);
  if (it == cur_map_.end()) {
    features->set_value(fid_, 1);
    //cerr << "  BAD!\n";
    return;
  }
  SetStateMask(it->second, it->second + yield.size(), state);
}

