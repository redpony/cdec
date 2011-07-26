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
namespace boostpo = boost::program_options;


/*
 * init
 *
 */
bool
init(int argc, char** argv, boostpo::variables_map* conf)
{
  boostpo::options_description opts( "Options" );
  opts.add_options()
    ( "decoder-config,c", boostpo::value<string>(), "configuration file for cdec" )
    ( "kbest,k",          boostpo::value<size_t>(), "k for kbest" )
    ( "ngrams,n",         boostpo::value<int>(),    "n for Ngrams" )
    ( "filter,f",         boostpo::value<string>(), "filter kbest list" )
    ( "test",                                       "run tests and exit");
  boostpo::options_description cmdline_options;
  cmdline_options.add(opts);
  boostpo::store( parse_command_line(argc, argv, cmdline_options), *conf );
  boostpo::notify( *conf );
  if ( ! (conf->count("decoder-config") || conf->count("test")) ) {
    cerr << cmdline_options << endl;
    return false;
  }
  return true;
}


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

  KBestList* getkb() { return &kb; }

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
 * write_training_data_for_sofia
 *
 */
void
sofia_write_training_data()
{
  // TODO
}


/*
 * call_sofia
 *
 */
void
sofia_call()
{
  // TODO
}


/*
 * sofia_model2weights
 *
 */
void
sofia_read_model()
{
  // TODO
}


/*
 * make_ngrams
 *
 */
typedef map<vector<WordID>, size_t> Ngrams;
Ngrams
make_ngrams( vector<WordID>& s, size_t N )
{
  Ngrams ngrams;
  vector<WordID> ng;
  for ( size_t i = 0; i < s.size(); i++ ) {
    ng.clear();
    for ( size_t j = i; j < min( i+N, s.size() ); j++ ) {
      ng.push_back( s[j] );
      ngrams[ng]++;
    }
  }
  return ngrams;
}


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


/*
 * ngram_matches
 *
 */
NgramCounts
make_ngram_counts( vector<WordID> hyp, vector<WordID> ref, size_t N )
{
  Ngrams hyp_ngrams = make_ngrams( hyp, N );
  Ngrams ref_ngrams = make_ngrams( ref, N );
  NgramCounts counts( N );
  Ngrams::iterator it;
  Ngrams::iterator ti;
  for ( it = hyp_ngrams.begin(); it != hyp_ngrams.end(); it++ ) {
    ti = ref_ngrams.find( it->first );
    if ( ti != ref_ngrams.end() ) {
      counts.add( it->second, ti->second, it->first.size() - 1 );
    } else {
      counts.add( it->second, 0, it->first.size() - 1 );
    }
  }
  return counts;
}


/*
 * brevity_penaly
 *
 */
double
brevity_penaly( const size_t hyp_len, const size_t ref_len )
{
  if ( hyp_len > ref_len ) return 1;
  return exp( 1 - (double)ref_len/(double)hyp_len );
}


/*
 * bleu
 * as in "BLEU: a Method for Automatic Evaluation of Machine Translation" (Papineni et al. '02)
 * page TODO
 * 0 if for N one of the counts = 0
 */
double
bleu( NgramCounts& counts, const size_t hyp_len, const size_t ref_len,
      size_t N, vector<float> weights = vector<float>() )
{
  if ( hyp_len == 0 || ref_len == 0 ) return 0;
  if ( ref_len < N ) N = ref_len;
  float N_ = (float)N;
  if ( weights.empty() )
  {
    for ( size_t i = 0; i < N; i++ ) weights.push_back( 1/N_ );
  }
  double sum = 0;
  for ( size_t i = 0; i < N; i++ ) {
    if ( counts.clipped[i] == 0 || counts.sum[i] == 0 ) return 0;
    sum += weights[i] * log( (double)counts.clipped[i] / (double)counts.sum[i] );
  }
  return brevity_penaly( hyp_len, ref_len ) * exp( sum );
}


/*
 * stupid_bleu
 * as in "ORANGE: a Method for Evaluating Automatic Evaluation Metrics for Machine Translation (Lin & Och '04)
 * page TODO
 * 0 iff no 1gram match
 */
double
stupid_bleu( NgramCounts& counts, const size_t hyp_len, const size_t ref_len,
             size_t N, vector<float> weights = vector<float>() )
{
  if ( hyp_len == 0 || ref_len == 0 ) return 0;
  if ( ref_len < N ) N = ref_len;
  float N_ = (float)N;
  if ( weights.empty() )
  {
    for ( size_t i = 0; i < N; i++ ) weights.push_back( 1/N_ );
  }
  double sum = 0;
  float add = 0;
  for ( size_t i = 0; i < N; i++ ) {
    if ( i == 1 ) add = 1;
    sum += weights[i] * log( ((double)counts.clipped[i] + add) / ((double)counts.sum[i] + add) );
  }
  return brevity_penaly( hyp_len, ref_len ) * exp( sum );
}


/*
 * smooth_bleu
 * as in "An End-to-End Discriminative Approach to Machine Translation" (Liang et al. '06)
 * page TODO
 * max. 0.9375
 */
double
smooth_bleu( NgramCounts& counts, const size_t hyp_len, const size_t ref_len,
             const size_t N, vector<float> weights = vector<float>() )
{
  if ( hyp_len == 0 || ref_len == 0 ) return 0;
  float N_ = (float)N;
  if ( weights.empty() )
  {
    for ( size_t i = 0; i < N; i++ ) weights.push_back( 1/N_ );
  }
  double sum = 0;
  float j = 1;
  for ( size_t i = 0; i < N; i++ ) {
    if ( counts.clipped[i] == 0 || counts.sum[i] == 0) continue;
    sum += exp((weights[i] * log((double)counts.clipped[i]/(double)counts.sum[i]))) / pow( 2, N_-j+1 );
    j++;
  }
  return brevity_penaly( hyp_len, ref_len ) * sum;
}


/*
 * approx_bleu
 * as in "Online Large-Margin Training for Statistical Machine Translation" (Watanabe et al. '07)
 * page TODO
 *
 */
double
approx_bleu( NgramCounts& counts, const size_t hyp_len, const size_t ref_len,
     const size_t N, vector<float> weights = vector<float>() )
{
  return bleu( counts, hyp_len, ref_len, N, weights );
}


/*
 * register_and_convert
 *
 */
void
register_and_convert(const vector<string>& strs, vector<WordID>& ids)
{
  vector<string>::const_iterator it;
  for ( it = strs.begin(); it < strs.end(); it++ ) {
    ids.push_back( TD::Convert( *it ) );
  }
}


/*
 *
 *
 */
void
test_ngrams()
{
  cout << "Testing ngrams..." << endl << endl;
  size_t N = 5;
  cout << "N = " << N << endl;
  vector<int> a; // hyp
  vector<int> b; // ref
  cout << "a ";
  for (size_t i = 1; i <= 8; i++) {
    cout << i << " ";
    a.push_back(i);
  }
  cout << endl << "b ";
  for (size_t i = 1; i <= 4; i++) {
    cout << i << " ";
    b.push_back(i);
  }
  cout << endl << endl;
  NgramCounts c = make_ngram_counts( a, b, N );
  assert( c.clipped[N-1] == 0 );
  assert( c.sum[N-1] == 4 );
  c.print();
  c += c;
  cout << endl;
  c.print();
  cout << endl;
}


/*
 *
 *
 */
double
approx_equal( double x, double y )
{
  const double EPSILON = 1E-5;
  if ( x == 0 ) return fabs( y ) <= EPSILON;
  if ( y == 0 ) return fabs( x ) <= EPSILON;
  return fabs( x - y ) / max( fabs(x), fabs(y) ) <= EPSILON;
}


/*
 *
 *
 */
#include <boost/assign/std/vector.hpp>
#include <iomanip>
void
test_metrics()
{
  cout << "Testing metrics..." << endl << endl;
  using namespace boost::assign;
  vector<string> a, b;
  vector<double> expect_vanilla, expect_smooth, expect_stupid;
  a +=              "a a a a", "a a a a", "a",   "a", "b",        "a a a a", "a a",  "a a a", "a b a"; // hyp
  b +=              "b b b b", "a a a a", "a",   "b", "b b b b",  "a",       "a a",  "a a a", "a b b"; // ref
  expect_vanilla += 0,         1,         1,      0,  0,          .25,       1,      1,       0;
  expect_smooth  += 0,          .9375,     .0625, 0,   .00311169, .0441942,   .1875,  .4375,   .161587;
  expect_stupid  += 0,         1,         1,      0,   .0497871,  .25,       1,      1,        .605707;
  vector<string> aa, bb;
  vector<WordID> aai, bbi;
  double vanilla, smooth, stupid;
  size_t N = 4;
  cout << "N = " << N << endl << endl;
  for ( size_t i = 0; i < a.size(); i++ ) {
    cout << " hyp: " << a[i] << endl;
    cout << " ref: " << b[i] << endl;
    aa.clear(); bb.clear(); aai.clear(); bbi.clear();
    boost::split( aa, a[i], boost::is_any_of(" ") );
    boost::split( bb, b[i], boost::is_any_of(" ") );
    register_and_convert( aa, aai );
    register_and_convert( bb, bbi );
    NgramCounts counts = make_ngram_counts( aai, bbi, N );
    vanilla =        bleu( counts, aa.size(), bb.size(), N);
    smooth  = smooth_bleu( counts, aa.size(), bb.size(), N);
    stupid  = stupid_bleu( counts, aa.size(), bb.size(), N);
    assert( approx_equal(vanilla, expect_vanilla[i]) );
    assert( approx_equal(smooth, expect_smooth[i]) );
    assert( approx_equal(stupid, expect_stupid[i]) );
    cout << setw(14) << "bleu = "      << vanilla << endl;
    cout << setw(14) << "smooth bleu = " << smooth << endl;
    cout << setw(14) << "stupid bleu = " << stupid << endl << endl;
  }
  cout << endl;
}

/*
 *
 *
 */
void
test_SetWeights()
{
  cout << "Testing Weights::SetWeight..." << endl << endl;
  Weights weights;
  SparseVector<double> lambdas;
  weights.InitSparseVector( &lambdas );
  weights.SetWeight( &lambdas, "test", 0 );
  weights.SetWeight( &lambdas, "test1", 1 );
  WordID fid = FD::Convert( "test2" );
  weights.SetWeight( &lambdas, fid, 2 );
  string fn = "weights-test";
  cout << "FD::NumFeats() " << FD::NumFeats() << endl;
  assert( FD::NumFeats() == 4 );
  weights.WriteToFile( fn, true );
  cout << endl;
}


/*
 *
 *
 */
void
run_tests()
{
  cout << endl;
  test_ngrams();
  cout << endl;
  test_metrics();
  cout << endl;
  test_SetWeights();
  exit(0);
}


void
print_FD()
{
  for ( size_t i = 0; i < FD::NumFeats(); i++ ) cout << FD::Convert(i)<< endl;
}


/*
 * main
 *
 */
int
main(int argc, char** argv)
{
  //SetSilent(true);
  boostpo::variables_map conf;
  if (!init(argc, argv, &conf)) return 1;
  if ( conf.count("test") ) run_tests(); 
  register_feature_functions();
  size_t k = conf["kbest"].as<size_t>();
  ReadFile ini_rf(conf["decoder-config"].as<string>());
  Decoder decoder(ini_rf.stream());
  KBestGetter observer(k);
  
  // for approx. bleu
  //NgramCounts global_counts;
  //size_t global_hyp_len;
  //size_t global_ref_len;

  Weights weights;
  SparseVector<double> lambdas;
  weights.InitSparseVector(&lambdas);
  vector<double> dense_weights;

  lambdas.set_value(FD::Convert("logp"), 0);

 
  vector<string> strs;
  string in, psg;
  size_t i = 0;
  while( getline(cin, in) ) {
    if ( !SILENT ) cerr << endl << endl << "Getting kbest for sentence #" << i << endl;
    // why? why!?
    dense_weights.clear();
    weights.InitFromVector( lambdas );
    weights.InitVector( &dense_weights );
    decoder.SetWeights( dense_weights );
    //cout << "use_shell " << dense_weights[FD::Convert("use_shell")] << endl;
    strs.clear();
    boost::split( strs, in, boost::is_any_of("\t") );
    psg = boost::replace_all_copy( strs[2], " __NEXT_RULE__ ", "\n" ); psg += "\n";
    //decoder.SetId(i);
    decoder.SetSentenceGrammar( psg );
    decoder.Decode( strs[0], &observer );
    KBestList* kb = observer.getkb();
    for ( size_t i = 0; i < k; i++ ) {
      cout << i << " ";
      for (size_t j = 0; j < kb->sents[i].size(); ++j ) {
        cout << TD::Convert( kb->sents[i][j] ) << " ";
      }
      cout << kb->scores[i];
      cout << endl;
    }
    lambdas.set_value( FD::Convert("use_shell"), 1 );
    lambdas.set_value( FD::Convert("use_a"), 1 );
    //print_FD();
  }
  
  weights.WriteToFile( "weights-final", true );

  return 0;
}

    // next: FMap, ->sofia, ->FMap, -> Weights
    // learner gets all used features (binary! and dense (logprob is sum of logprobs!))
    // only for those feats with weight > 0 after learning
    // see decoder line 548


/*
 * TODO
 *  iterate over training set, for t=1..T
 *  mapred impl
 *   mapper:  main
 *   reducer: average weights, global NgramCounts for approx. bleu
 *  1st cut: hadoop streaming?
 *  batch, non-batch in the mapper (what sofia gets, regenerated Kbest lists)
 *  filter kbest yes/no
 *  sofia: --eta_type explicit
 *  psg preparation source\tref\tpsg
 *  set reference for cdec?
 *  LM
 *   shared?
 *   startup?
 *  X reference(s) for *bleu!?
 *  kbest nicer (do not iterate twice)!? -> shared_ptr
 *  multipartite ranking
 *  weights! global, per sentence from global, featuremap
 *  const decl...
 *  sketch: batch/iter options
 *  weights.cc: why wv_?
 *  --weights cmd line (for iterations): script to call again/hadoop streaming?
 *  I do not need to remember features, cdec does
 *  resocre hg?
 *  do not use Decoder::Decode!?
 *  what happens if feature not in FD? 0???
 */

/*
 * PROBLEMS
 *  cdec kbest vs 1best (no -k param)
 *  FD, Weights::wv_ grow too large, see utils/weights.cc; decoder/hg.h; decoder/scfg_translator.cc; utils/fdict.cc!?
 *  sparse vector instead of vector<double> for weights in Decoder?
 *  PhraseModel_* features for psg!? (seem to be generated)
 */

