#include "tests.h"


namespace dtrain
{


/*
 * approx_equal
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
 * test_ngrams
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
 * test_metrics
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
 * test_SetWeights
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
 * run_tests
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


} // namespace

