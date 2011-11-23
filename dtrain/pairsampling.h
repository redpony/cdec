#ifndef _DTRAIN_PAIRSAMPLING_H_
#define _DTRAIN_PAIRSAMPLING_H_

namespace dtrain
{


inline void
all_pairs(vector<ScoredHyp>* s, vector<pair<ScoredHyp,ScoredHyp> >& training)
{
  for (unsigned i = 0; i < s->size()-1; i++) {
    for (unsigned j = i+1; j < s->size(); j++) {
      pair<ScoredHyp,ScoredHyp> p;
      p.first = (*s)[i];
      p.second = (*s)[j];
      training.push_back(p);
    }
  }
}

inline void
rand_pairs_5050(vector<ScoredHyp>* s, vector<pair<ScoredHyp,ScoredHyp> >& training,
                  MT19937* prng)
{
  for (unsigned i = 0; i < s->size()-1; i++) {
    for (unsigned j = i+1; j < s->size(); j++) {
      if (prng->next() < .5) {
        pair<ScoredHyp,ScoredHyp> p;
        p.first = (*s)[i];
        p.second = (*s)[j];
        training.push_back(p);
      }
    }
  }
}

bool
_multpart_cmp_hyp_by_score(ScoredHyp a, ScoredHyp b)
{
  return a.score < b.score;
}
inline void
multpart108010(vector<ScoredHyp>* s, vector<pair<ScoredHyp,ScoredHyp> >& training)
{
  sort(s->begin(), s->end(), _multpart_cmp_hyp_by_score);
  pair<ScoredHyp,ScoredHyp>  p;
  unsigned sz = s->size();
  unsigned slice = 10;
  unsigned sep = sz%slice;
  if (sep == 0) sep = sz/slice;
  for (unsigned i = 0; i < sep; i++) {
    for(unsigned j = sep; j < sz; j++) {
      p.first = (*s)[i];
      p.second = (*s)[j];
      if(p.first.rank < p.second.rank) training.push_back(p);
    }
  }
  for (unsigned i = sep; i < sz-sep; i++) {
    for (unsigned j = sz-sep; j < sz; j++) {
      p.first = (*s)[i];
      p.second = (*s)[j];
      if(p.first.rank < p.second.rank) training.push_back(p);
    }
  }
}


inline bool
_PRO_accept_pair(pair<ScoredHyp,ScoredHyp> &p)
{
  if (fabs(p.first.score - p.second.score) < 0.05) return false;
  return true;
}
bool
_PRO_cmp_pair_by_diff(pair<ScoredHyp,ScoredHyp> a, pair<ScoredHyp,ScoredHyp> b)
{
  // descending order
  return (fabs(a.first.score - a.second.score)) > (fabs(b.first.score - b.second.score));
}
inline void
PROsampling(vector<ScoredHyp>* s, vector<pair<ScoredHyp,ScoredHyp> >& training) // ugly
{
  unsigned max_count = 5000, count = 0;
  bool b = false;
  for (unsigned i = 0; i < s->size()-1; i++) {
    for (unsigned j = i+1; j < s->size(); j++) {
      pair<ScoredHyp,ScoredHyp> p;
      p.first = (*s)[i];
      p.second = (*s)[j];
      if (_PRO_accept_pair(p)) {
        training.push_back(p);
        count++;
        if (count == max_count) {
          b = true;
          break;
        }
      }
    }
    if (b) break;
  }
  sort(training.begin(), training.end(), _PRO_cmp_pair_by_diff);
  if (training.size() > 50)
    training.erase(training.begin()+50, training.end()); 
  return;
}

inline void
all_pairs_discard(vector<ScoredHyp>* s, vector<pair<ScoredHyp,ScoredHyp> >& training)
{
  for (unsigned i = 0; i < s->size()-1; i++) {
    for (unsigned j = i+1; j < s->size(); j++) {
      pair<ScoredHyp,ScoredHyp> p;
      p.first = (*s)[i];
      p.second = (*s)[j];
      if(_PRO_accept_pair(p))
        training.push_back(p);
    }
  }
}


} // namespace

#endif

