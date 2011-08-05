#include "score.h"


namespace dtrain
{


/******************************************************************************
 * NGRAMS
 *
 *
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


/******************************************************************************
 * SCORES
 *
 *
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
 * CHIANG, RESNIK, synt struct features
 * .9*
 * page TODO
 *
 */
double
approx_bleu( NgramCounts& counts, const size_t hyp_len, const size_t ref_len,
     const size_t N, vector<float> weights )
{
  return bleu( counts, hyp_len, ref_len, N, weights );
}


} // namespace

