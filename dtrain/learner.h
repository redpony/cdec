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
    //unlink( "/tmp/sofia_ml_training_stupid" );
    o.open( "/tmp/sofia_ml_training_normalx", ios::trunc ); // TODO randomize, filename exists
    int fid = 0;
    map<int,int>::iterator ff;

    for ( size_t k = 0; k < kbest.size(); ++k ) {
      map<int,double> m;
      SparseVector<double>::const_iterator it = kbest[k].begin();
      o << scores[k].GetScore();
      for ( ; it != kbest[k].end(); ++it) {
        ff = fmap.find( it->first );
        if ( ff == fmap.end() ) {
          fmap.insert( pair<int,int>(it->first, fid) );
          fmap1.insert( pair<int,int>(fid, it->first) );
          fid++;
        }
        m.insert(pair<int,double>(fmap[it->first], it->second));
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
    string call = "./sofia-ml --training_file /tmp/sofia_ml_training_normalx --model_out /tmp/sofia_ml_model_normalx --loop_type stochastic --lambda 100 --dimensionality ";
    std::stringstream out;
    out << fmap.size();
    call += out.str();
    call += " &>/dev/null";
    system ( call.c_str() );
    ifstream i;
    //unlink( "/tmp/sofia_ml_model_stupid" );
    i.open( "/tmp/sofia_ml_model_normalx", ios::in );
    string model;
    getline( i, model );
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

