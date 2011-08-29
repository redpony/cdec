#ifndef _DTRAIN_LEARNER_H_
#define _DTRAIN_LEARNER_H_

#include <string>
#include <vector>
#include <map>

#include "sparse_vector.h"
#include "score.h"


namespace dtrain
{


class Updater
{
  public:
    virtual void Init( const vector<SparseVector<double> >& kbest, const Scores& scores,
                       const bool invert_score = false ) {};
    virtual void Update( SparseVector<double>& lambdas ) {};
};


class SofiaUpdater : public Updater
{
  public:
    void
    Init( const size_t sid, const vector<SparseVector<double> >& kbest, /*const FIXME operator[]*/ Scores& scores,
          const bool invert_score = false )
    {
      assert( kbest.size() == scores.size() );
      ofstream o;
      char tmp[] = DTRAIN_TMP_DIR"/dtrain-sofia-data-XXXXXX";
      mkstemp( tmp );
      tmp_data_fn = tmp;
      o.open( tmp_data_fn.c_str(), ios::trunc );
      int fid = 0;
      map<int,int>::iterator ff;
      double score;
      for ( size_t k = 0; k < kbest.size(); ++k ) {
        map<int,double> m;
        SparseVector<double>::const_iterator it = kbest[k].begin();
        score = scores[k].GetScore();
        if ( invert_score ) score = -score;
        o << score;
        for ( ; it != kbest[k].end(); ++it ) {
          ff = fmap.find( it->first );
          if ( ff == fmap.end() ) {
            fmap.insert( pair<int,int>(it->first, fid) );
            fmap1.insert( pair<int,int>(fid, it->first) );
            fid++;
          }
          m.insert( pair<int,double>(fmap[it->first], it->second) );
        }
        map<int,double>::iterator ti = m.begin();
        for ( ; ti != m.end(); ++ti ) {
          o << " " << ti->first << ":" << ti->second;
        }
        o << endl;
      }
      o.close();
    }

    void
    Update(SparseVector<double>& lambdas)
    {
      char tmp[] = DTRAIN_TMP_DIR"/dtrain-sofia-model-XXXXXX";
      mkstemp(tmp);
      tmp_model_fn = tmp;
      string call = "./sofia-ml --training_file " + tmp_data_fn;
      call += " --model_out " + tmp_model_fn;
      call += " --loop_type stochastic --lambda 100 --dimensionality ";
      std::stringstream out;
      out << fmap.size();
      call += out.str();
      call += " &>/dev/null";
      system ( call.c_str() );
      ifstream i;
      i.open( tmp_model_fn.c_str(), ios::in );
      string model;
      getline( i, model );
      vector<string> strs;
      boost::split( strs, model, boost::is_any_of(" ") );
      int j = 0;
      for ( vector<string>::iterator it = strs.begin(); it != strs.end(); ++it ) {
        lambdas.set_value(fmap1[j], atof( it->c_str() ) );
        j++;
      }
      i.close();
      unlink( tmp_data_fn.c_str() );
      unlink( tmp_model_fn.c_str() );
    }

  private:
    string tmp_data_fn;
    string tmp_model_fn;
    map<int,int> fmap;
    map<int,int> fmap1;
};


} // namespace

#endif

