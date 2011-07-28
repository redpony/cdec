#include "dcommon.h"



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
      size_t N, vector<float> weights  )
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
             size_t N, vector<float> weights  )
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
             const size_t N, vector<float> weights  )
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
     const size_t N, vector<float> weights )
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

