#include "kbestget.h"


namespace dtrain
{


struct TPair
{
  double type;
  SparseVector<double> first;
  SparseVector<double> second;
};

typedef vector<TPair> TrainingInstances;


void
sample_all( KBestList* kb, TrainingInstances &training )
{
  double type;
  for ( size_t i = 0; i < kb->GetSize()-1; i++ ) {
   for ( size_t j = i+1; j < kb->GetSize(); j++ ) {
     if ( kb->scores[i] - kb->scores[j] < 0 ) {
       type = -1; 
     } else {
       type = 1;
     }
     TPair p;
     p.type = type;
     p.first = kb->feats[i];
     p.second = kb->feats[j];
     training.push_back( p );
   }
 }
}

/*void
sample_all_only_neg(, vector<pair<SparSparseVector<double> > pairs)
{

}

void
sample_random_pos()
{
  if ( rand() % 2 ) { // sample it?
}*/


} // namespace

