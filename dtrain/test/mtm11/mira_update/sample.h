#ifndef _DTRAIN_SAMPLE_H_
#define _DTRAIN_SAMPLE_H_


#include "kbestget.h"


namespace dtrain
{


struct TPair
{
  SparseVector<double> first, second;
  size_t first_rank, second_rank;
  double first_score, second_score;
  double model_score_diff;
  double loss_diff;
};

typedef vector<TPair> TrainingInstances;


void
  sample_all( KBestList* kb, TrainingInstances &training, size_t n_pairs )
{
  std::vector<double> loss_diffs;
  TrainingInstances training_tmp;
  for ( size_t i = 0; i < kb->GetSize()-1; i++ ) {
    for ( size_t j = i+1; j < kb->GetSize(); j++ ) {
      TPair p;
      p.first = kb->feats[i];
      p.second = kb->feats[j];
      p.first_rank = i;
      p.second_rank = j;
      p.first_score = kb->scores[i];
      p.second_score = kb->scores[j];

      bool conservative = 1;
      if ( kb->scores[i] - kb->scores[j] < 0 ) {
	// j=hope, i=fear                                                                                                                         
	p.model_score_diff = kb->model_scores[j] - kb->model_scores[i];
        p.loss_diff = kb->scores[j] - kb->scores[i];
        training_tmp.push_back(p);
        loss_diffs.push_back(p.loss_diff);
      }
      else if (!conservative) {
	// i=hope, j=fear
	p.model_score_diff = kb->model_scores[i] - kb->model_scores[j];
        p.loss_diff = kb->scores[i] - kb->scores[j];
        training_tmp.push_back(p);
        loss_diffs.push_back(p.loss_diff);
      }
    }
  }
  
  if (training_tmp.size() > 0) {
    double threshold;
    std::sort(loss_diffs.begin(), loss_diffs.end());
    std::reverse(loss_diffs.begin(), loss_diffs.end());
    threshold = loss_diffs.size() >= n_pairs ? loss_diffs[n_pairs-1] : loss_diffs[loss_diffs.size()-1];
    cerr << "threshold: " << threshold << endl;
    size_t constraints = 0;
    for (size_t i = 0; (i < training_tmp.size() && constraints < n_pairs); ++i) {
      if (training_tmp[i].loss_diff >= threshold) {
	training.push_back(training_tmp[i]);
	constraints++;
      }
    }
  }
  else {
    cerr << "No pairs selected." << endl;
  }
}

void
sample_rand( KBestList* kb, TrainingInstances &training )
{
  srand( time(NULL) );
  for ( size_t i = 0; i < kb->GetSize()-1; i++ ) {
    for ( size_t j = i+1; j < kb->GetSize(); j++ ) {
      if ( rand() % 2 ) {
        TPair p;
        p.first = kb->feats[i];
        p.second = kb->feats[j];
        p.first_rank = i;
        p.second_rank = j;
        p.first_score = kb->scores[i];
        p.second_score = kb->scores[j];
        training.push_back( p );
      }
    }
  }
}


} // namespace


#endif

