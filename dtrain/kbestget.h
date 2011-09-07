#ifndef _DTRAIN_KBESTGET_H_
#define _DTRAIN_KBESTGET_H_

#include "kbest.h"

namespace dtrain
{


/*
 * KBestList
 *
 */
struct KBestList {
  vector<SparseVector<double> > feats;
  vector<vector<WordID> > sents;
  vector<double> model_scores;
  vector<double> scores;
  size_t GetSize() { return sents.size(); }
};


/*
 * KBestGetter
 *
 */
struct KBestGetter : public DecoderObserver
{
  const size_t k_;
  const string filter_type;
  KBestList kb;

  KBestGetter( const size_t k, const string filter_type ) :
    k_(k), filter_type(filter_type) {}

  virtual void
  NotifyTranslationForest( const SentenceMetadata& smeta, Hypergraph* hg )
  {
    KBest( *hg );
  }

  KBestList* GetKBest() { return &kb; }

  void
  KBest( const Hypergraph& forest )
  {
    if ( filter_type == "unique" ) {
      KBestUnique( forest );
    } else if ( filter_type == "no" ) {
      KBestNoFilter( forest );
    }
  }

  void
  KBestUnique( const Hypergraph& forest )
  {
    kb.sents.clear();
    kb.feats.clear();
    kb.model_scores.clear();
    kb.scores.clear();
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal, KBest::FilterUnique, prob_t, EdgeProb> kbest( forest, k_ );
    for ( size_t i = 0; i < k_; ++i ) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal, KBest::FilterUnique, prob_t, EdgeProb>::Derivation* d =
            kbest.LazyKthBest( forest.nodes_.size() - 1, i );
      if (!d) break;
      kb.sents.push_back( d->yield);
      kb.feats.push_back( d->feature_values );
      kb.model_scores.push_back( d->score );
    }
  }

  void
  KBestNoFilter( const Hypergraph& forest )
  {
    kb.sents.clear();
    kb.feats.clear();
    kb.model_scores.clear();
    kb.scores.clear();
    KBest::KBestDerivations<vector<WordID>, ESentenceTraversal> kbest( forest, k_ );
    for ( size_t i = 0; i < k_; ++i ) {
      const KBest::KBestDerivations<vector<WordID>, ESentenceTraversal>::Derivation* d =
            kbest.LazyKthBest( forest.nodes_.size() - 1, i );
      if (!d) break;
      kb.sents.push_back( d->yield);
      kb.feats.push_back( d->feature_values );
      kb.model_scores.push_back( d->score );
    }
  }
};


} // namespace


#endif

