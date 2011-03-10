namespace {
char const* bleu_usage_name="BLEUModel";
char const* bleu_usage_short="[-o 3|4]";
char const* bleu_usage_verbose="Uses feature id 0!  Make sure there are no other features whose weights aren't specified or there may be conflicts.  Computes oracle with weighted combination of BLEU and model score (from previous model set, using weights on edges?).  Performs ngram context expansion; expect reference translation info in sentence metadata; if document scorer is IBM_BLEU_3, then use order 3; otherwise use order 4.";
}


#include <sstream>
#include <unistd.h>
#include "fast_lexical_cast.hpp"

#include <boost/shared_ptr.hpp>

#include "ff_bleu.h"
#include "tdict.h"
#include "hg.h"
#include "stringlib.h"
#include "sentence_metadata.h"
#include "scorer.h"

using namespace std;

class BLEUModelImpl {
 public:
  explicit BLEUModelImpl(int order) :
      buffer_(), order_(order), state_size_(OrderToStateSize(order) - 1),
      floor_(-100.0),
      kSTART(TD::Convert("<s>")),
      kSTOP(TD::Convert("</s>")),
      kUNKNOWN(TD::Convert("<unk>")),
      kNONE(-1),
      kSTAR(TD::Convert("<{STAR}>")) {}

  virtual ~BLEUModelImpl() {
      }

  inline int StateSize(const void* state) const {
    return *(static_cast<const char*>(state) + state_size_);
  }

  inline void SetStateSize(int size, void* state) const {
    *(static_cast<char*>(state) + state_size_) = size;
  }

  void GetRefToNgram()
  {}

  string DebugStateToString(const void* state) const {
    int len = StateSize(state);
    const int* astate = reinterpret_cast<const int*>(state);
    string res = "[";
    for (int i = 0; i < len; ++i) {
      res += " ";
      res += TD::Convert(astate[i]);
    }
    res += " ]";
    return res;
  }

  inline double ProbNoRemnant(int i, int len) {
    int edge = len;
    bool flag = true;
    double sum = 0.0;
    while (i >= 0) {
      if (buffer_[i] == kSTAR) {
        edge = i;
        flag = false;
      } else if (buffer_[i] <= 0) {
        edge = i;
        flag = true;
      } else {
        if ((edge-i >= order_) || (flag && !(i == (len-1) && buffer_[i] == kSTART)))
	  {          //sum += LookupProbForBufferContents(i);
	    //cerr << "FT";
	    CalcPhrase(buffer_[i], &buffer_[i+1]);
	  }
      }
      --i;
    }
    return sum;
  }

  double FinalTraversalCost(const void* state) {
    int slen = StateSize(state);
    int len = slen + 2;
    // cerr << "residual len: " << len << endl;
    buffer_.resize(len + 1);
    buffer_[len] = kNONE;
    buffer_[len-1] = kSTART;
    const int* astate = reinterpret_cast<const int*>(state);
    int i = len - 2;
    for (int j = 0; j < slen; ++j,--i)
      buffer_[i] = astate[j];
    buffer_[i] = kSTOP;
    assert(i == 0);
    return ProbNoRemnant(len - 1, len);
  }

  vector<WordID> CalcPhrase(int word, int* context) {
     int i = order_;
    vector<WordID> vs;
    int c = 1;
    vs.push_back(word);
    // while (i > 1 && *context > 0) {
     while (*context > 0) {
      --i;
      vs.push_back(*context);
      ++context;
      ++c;
    }
     if(false){	cerr << "VS1( ";
	vector<WordID>::reverse_iterator rit;
	for ( rit=vs.rbegin() ; rit != vs.rend(); ++rit )
	  cerr << " " << TD::Convert(*rit);
	cerr << ")\n";}

    return vs;
  }


  double LookupWords(const TRule& rule, const vector<const void*>& ant_states, void* vstate, const SentenceMetadata& smeta) {

    int len = rule.ELength() - rule.Arity();

    for (int i = 0; i < ant_states.size(); ++i)
      len += StateSize(ant_states[i]);
    buffer_.resize(len + 1);
    buffer_[len] = kNONE;
    int i = len - 1;
    const vector<WordID>& e = rule.e();

    /*cerr << "RULE::" << rule.ELength() << " ";
    for (vector<WordID>::const_iterator i = e.begin(); i != e.end(); ++i)
      {
	const WordID& c = *i;
	if(c > 0) cerr << TD::Convert(c) << "--";
	else cerr <<"N--";
      }
    cerr << endl;
    */

    for (int j = 0; j < e.size(); ++j) {
      if (e[j] < 1) {
        const int* astate = reinterpret_cast<const int*>(ant_states[-e[j]]);
        int slen = StateSize(astate);
        for (int k = 0; k < slen; ++k)
          buffer_[i--] = astate[k];
      } else {
        buffer_[i--] = e[j];
      }
    }

    double approx_bleu = 0.0;
    int* remnant = reinterpret_cast<int*>(vstate);
    int j = 0;
    i = len - 1;
    int edge = len;


    vector<WordID> vs;
    while (i >= 0) {
      vs = CalcPhrase(buffer_[i],&buffer_[i+1]);
      if (buffer_[i] == kSTAR) {
        edge = i;
      } else if (edge-i >= order_) {

	vs = CalcPhrase(buffer_[i],&buffer_[i+1]);

      } else if (edge == len && remnant) {
        remnant[j++] = buffer_[i];
      }
      --i;
    }

    //calculate Bvector here
    /* cerr << "VS1( ";
    vector<WordID>::reverse_iterator rit;
    for ( rit=vs.rbegin() ; rit != vs.rend(); ++rit )
      cerr << " " << TD::Convert(*rit);
    cerr << ")\n";
    */

    ScoreP node_score_p = smeta.GetDocScorer()[smeta.GetSentenceID()]->ScoreCCandidate(vs);
    Score *node_score=node_score_p.get();
    string details;
    node_score->ScoreDetails(&details);
    const Score *base_score= &smeta.GetScore();
    //cerr << "SWBASE : " << base_score->ComputeScore() << details << " ";

    int src_length = smeta.GetSourceLength();
    node_score->PlusPartialEquals(*base_score, rule.EWords(), rule.FWords(), src_length );
    float oracledoc_factor = (src_length + smeta.GetDocLen())/  src_length;

    //how it seems to be done in code
    //TODO: might need to reverse the -1/+1 of the oracle/neg examples
    //TO VLADIMIR: the polarity would be reversed if you switched error (1-BLEU) for BLEU.
    approx_bleu = ( rule.FWords() * oracledoc_factor  ) * node_score->ComputeScore();
    //how I thought it was done from the paper
    //approx_bleu = ( rule.FWords()+ smeta.GetDocLen() ) * node_score->ComputeScore();

    if (!remnant){  return approx_bleu;}

    if (edge != len || len >= order_) {
      remnant[j++] = kSTAR;
      if (order_-1 < edge) edge = order_-1;
      for (int i = edge-1; i >= 0; --i)
        remnant[j++] = buffer_[i];
    }

    SetStateSize(j, vstate);
    //cerr << "Return APPROX_BLEU: " << approx_bleu << " "<<  DebugStateToString(vstate) << endl;
    return approx_bleu;
  }

  static int OrderToStateSize(int order) {
    return ((order-1) * 2 + 1) * sizeof(WordID) + 1;
  }

 protected:
  vector<WordID> buffer_;
  const int order_;
  const int state_size_;
  const double floor_;

 public:
  const WordID kSTART;
  const WordID kSTOP;
  const WordID kUNKNOWN;
  const WordID kNONE;
  const WordID kSTAR;
};

string BLEUModel::usage(bool param,bool verbose) {
  return usage_helper(bleu_usage_name,bleu_usage_short,bleu_usage_verbose,param,verbose);
}

BLEUModel::BLEUModel(const string& param) :
  fid_(0) { //The partial BLEU score is kept in feature id=0
  vector<string> argv;
  int argc = SplitOnWhitespace(param, &argv);
  int order = 3;

  //loop over argv and load all references into vector of NgramMaps
  if (argc >= 1) {
    if (argv[0] != "-o" || argc<2) {
      cerr<<bleu_usage_name<<" specification should be: "<<bleu_usage_short<<"; you provided: "<<param<<endl<<argv[0]<<endl<<bleu_usage_verbose<<endl;
      abort();
    } else
      order=boost::lexical_cast<int>(argv[1]);
  }

  SetStateSize(BLEUModelImpl::OrderToStateSize(order));
  pimpl_ = new BLEUModelImpl(order);
}

BLEUModel::~BLEUModel() {
  delete pimpl_;
}

string BLEUModel::DebugStateToString(const void* state) const{
  return pimpl_->DebugStateToString(state);
}

void BLEUModel::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                          const Hypergraph::Edge& edge,
                                          const vector<const void*>& ant_states,
                                          SparseVector<double>* features,
                                          SparseVector<double>* /* estimated_features */,
                                          void* state) const {

  (void) smeta;
  /*cerr << "In BM calling set " << endl;
  const Score *s=  &smeta.GetScore();
  const int dl = smeta.GetDocLen();
  cerr << "SCO " << s->ComputeScore() << endl;
  const DocScorer *ds = &smeta.GetDocScorer();
  */

//  cerr<< "ff_bleu loading sentence " << smeta.GetSentenceID() << endl;
      //}
  features->set_value(fid_, pimpl_->LookupWords(*edge.rule_, ant_states, state, smeta));
  //cerr << "FID" << fid_ << " " << DebugStateToString(state) << endl;
}

void BLEUModel::FinalTraversalFeatures(const void* ant_state,
                                           SparseVector<double>* features) const {

  features->set_value(fid_, pimpl_->FinalTraversalCost(ant_state));
}
