#include <sstream>
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

#include "config.h"

#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "sentence_metadata.h"
#include "scorer.h"
#include "verbose.h"
#include "viterbi.h"
#include "hg.h"
#include "prob.h"
#include "kbest.h"
#include "ff_register.h"
#include "decoder.h"
#include "filelib.h"
#include "fdict.h"
#include "weights.h"
#include "sparse_vector.h"
#include "sampler.h"

using namespace std;
namespace po = boost::program_options;


struct ScorePair
{
  ScorePair(double modelscore, double score) : modelscore_(modelscore), score_(score) {} 
  double modelscore_, score_;
  double GetModelScore() { return modelscore_; }
  double GetScore() { return score_; }
};
typedef vector<ScorePair> Scores;


/*
 * KBestGetter
 *
 */
struct KBestList {
  vector<SparseVector<double> > feats;
  vector<vector<WordID> > sents;
  vector<double> scores;
};
struct KBestGetter : public DecoderObserver
{
  KBestGetter( const size_t k ) : k_(k) {}
  const size_t k_;
  KBestList kb;

  virtual void
  NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg)
  {
    GetKBest(smeta.GetSentenceID(), *hg);
  }

  KBestList* GetKBest() { return &kb; }

  void
  GetKBest(int sent_id, const Hypergraph& forest)
  {
    kb.scores.clear();
    kb.sents.clear();
    kb.feats.clear();
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest( forest, k_ );
    for ( size_t i = 0; i < k_; ++i ) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
        kbest.LazyKthBest( forest.nodes_.size() - 1, i );
      if (!d) break;
      kb.sents.push_back( d->yield);
      kb.feats.push_back( d->feature_values );
      kb.scores.push_back( d->score );
    }
  }
};


/*
 * NgramCounts
 *
 */
struct NgramCounts
{
  NgramCounts( const size_t N ) : N_( N ) {
    reset();
  } 
  size_t N_;
  map<size_t, size_t> clipped;
  map<size_t, size_t> sum;

  void
  operator+=( const NgramCounts& rhs )
  {
    assert( N_ == rhs.N_ );
    for ( size_t i = 0; i < N_; i++ ) {
      this->clipped[i] += rhs.clipped.find(i)->second;
      this->sum[i] += rhs.sum.find(i)->second;
    }
  }

  void
  add( size_t count, size_t ref_count, size_t i )
  {
    assert( i < N_ );
    if ( count > ref_count ) {
      clipped[i] += ref_count;
      sum[i] += count;
    } else {
      clipped[i] += count;
      sum[i] += count;
    }
  }

  void
  reset()
  {
    size_t i;
    for ( i = 0; i < N_; i++ ) {
      clipped[i] = 0;
      sum[i] = 0;
    }
  }

  void
  print()
  {
    for ( size_t i = 0; i < N_; i++ ) {
      cout << i+1 << "grams (clipped):\t" << clipped[i] << endl;
      cout << i+1 << "grams:\t\t\t" << sum[i] << endl;
    }
  }
};


/*class Learnerx
{
  public:
    virtual void Init(const vector<SparseVector<double> >& kbest, const Scores& scores) {};
    virtual void Update(SparseVector<double>& lambdas);
};*/

class SofiaLearner //: public Learnerx FIXME
{
  // TODO bool invert_score
  public:
  void
  Init( const size_t sid, const vector<SparseVector<double> >& kbest, /*const*/ Scores& scores )
  {
    assert( kbest.size() == scores.size() );
    ofstream o;
    unlink( "/tmo/sofia_ml_training" );
    o.open( "/tmp/sofia_ml_training", ios::trunc ); // TODO randomize, filename exists
    int fid = 0;
    map<int,int>::iterator ff;
    for ( size_t k = 0; k < kbest.size(); ++k ) {
      SparseVector<double>::const_iterator it = kbest[k].begin();
      o << scores[k].GetScore();
      for ( ; it != kbest[k].end(); ++it) {
        ff = fmap.find( it->first );
        if ( ff == fmap.end() ) {
          fmap.insert( pair<int,int>(it->first, fid) );
          fmap1.insert( pair<int,int>(fid, it->first) );
          fid++;
        }
        o << " "<< fmap[it->first] << ":" << it->second;
      }
      o << endl;
    }
    o.close();
  }

  void
  Update(SparseVector<double>& lambdas)
  {
    string call = "./sofia-ml --training_file /tmp/sofia_ml_training --model_out /tmp/sofia_ml_model --loop_type stochastic --lambda 100 --dimensionality ";
    std::stringstream out;
    out << fmap.size();
    call += out.str();
    call += " &>/dev/null";
    system ( call.c_str() );
    ifstream i;
    unlink( "/tmo/sofia_ml_model" );
    i.open( "/tmp/sofia_ml_model", ios::in );
    string model;
    getline( i, model );
    //cout << model << endl;
    vector<string> strs;
    boost::split( strs, model, boost::is_any_of(" ") );
    int j = 0;
    for ( vector<string>::iterator it = strs.begin(); it != strs.end(); ++it ) {
      lambdas.set_value(fmap1[j], atof( it->c_str() ) );
      j++;
    }

  }

  private:
    map<int,int> fmap;
    map<int,int> fmap1;
};

typedef map<vector<WordID>, size_t> Ngrams;
Ngrams make_ngrams( vector<WordID>& s, size_t N );
NgramCounts make_ngram_counts( vector<WordID> hyp, vector<WordID> ref, size_t N );
double brevity_penaly( const size_t hyp_len, const size_t ref_len );
double bleu( NgramCounts& counts, const size_t hyp_len, const size_t ref_len, size_t N, vector<float> weights = vector<float>() );
double stupid_bleu( NgramCounts& counts, const size_t hyp_len, const size_t ref_len, size_t N, vector<float> weights = vector<float>() );
double smooth_bleu( NgramCounts& counts, const size_t hyp_len, const size_t ref_len, const size_t N, vector<float> weights = vector<float>() );
double approx_bleu( NgramCounts& counts, const size_t hyp_len, const size_t ref_len, const size_t N, vector<float> weights = vector<float>() );
void register_and_convert(const vector<string>& strs, vector<WordID>& ids);




void print_FD();
void run_tests();
void test_SetWeights();
#include <boost/assign/std/vector.hpp>
#include <iomanip>
void test_metrics();
double approx_equal( double x, double y );
void test_ngrams();

