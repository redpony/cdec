#include "maxent.h"
#include <cmath>
#include <stdio.h>

using namespace std;

//const double SGD_ETA0 = 1;
//const double SGD_ITER = 30;
//const double SGD_ALPHA = 0.85;


//#define FOLOS_NAIVE
//#define FOLOS_LAZY
#define SGD_CP

inline void
apply_l1_penalty(const int i, const double u,
		 vector<double> & _vl, vector<double> & q)
{
  double & w = _vl[i];
  const double z = w;
  double & qi = q[i];
  if (w > 0) { 
    w = max(0.0, w - (u + qi));
  } else if (w < 0) {
    w = min(0.0, w + (u - qi));
  }
  qi += w - z;
}

static double
l1norm(const vector<double>& v)
{
  double sum = 0;
  for (size_t i = 0; i < v.size(); i++) sum += abs(v[i]);
  return sum;
}

inline void
update_folos_lazy(const int iter_sample,
		  const int k, vector<double> & _vl, const vector<double> & sum_eta,
		  vector<int> & last_updated)
{
  const double penalty = sum_eta[iter_sample] - sum_eta[last_updated[k]];
  double & x = _vl[k];
  if (x > 0) x = max(0.0, x - penalty);
  else       x = min(0.0, x + penalty);
  last_updated[k] = iter_sample;
}

int
ME_Model::perform_SGD()
{
  if (_l2reg > 0) {
    cerr << "error: L2 regularization is currently not supported in SGD mode." << endl;
    exit(1);
  }

  cerr << "performing SGD" << endl;

  const double l1param = _l1reg;

  const int d = _fb.Size();

  vector<int> ri(_vs.size());
  for (size_t i = 0; i < ri.size(); i++) ri[i] = i;

  vector<double> grad(d);
  int iter_sample = 0;
  const double eta0 = SGD_ETA0;

  //  cerr << "l1param = " << l1param << endl;
  cerr << "eta0 = " << eta0 << " alpha = " << SGD_ALPHA << endl;

  double u = 0;
  vector<double> q(d, 0);
  vector<int> last_updated(d, 0);
  vector<double> sum_eta;
  sum_eta.push_back(0);

  for (int iter = 0; iter < SGD_ITER; iter++) {

    random_shuffle(ri.begin(), ri.end());

    double logl = 0;
    int ncorrect = 0, ntotal = 0;
    for (size_t i = 0; i < _vs.size(); i++, ntotal++, iter_sample++) {
      const Sample & s = _vs[ri[i]];

#ifdef FOLOS_LAZY
      for (vector<int>::const_iterator j = s.positive_features.begin(); j != s.positive_features.end(); j++){
	for (vector<int>::const_iterator k = _feature2mef[*j].begin(); k != _feature2mef[*j].end(); k++) {
	  update_folos_lazy(iter_sample, *k, _vl, sum_eta, last_updated);
	}
      }
#endif

      vector<double> membp(_num_classes);
      const int max_label = conditional_probability(s, membp);

      const double eta = eta0 * pow(SGD_ALPHA, (double)iter_sample / _vs.size()); // exponential decay
      //      const double eta = eta0 / (1.0 + (double)iter_sample / _vs.size());

      //      if (iter_sample % _vs.size() == 0) cerr << "eta = " << eta << endl;
      u += eta * l1param;

      sum_eta.push_back(sum_eta.back() + eta * l1param);
    
      logl += log(membp[s.label]);
      if (max_label == s.label) ncorrect++;

      // binary features
      for (vector<int>::const_iterator j = s.positive_features.begin(); j != s.positive_features.end(); j++){
	for (vector<int>::const_iterator k = _feature2mef[*j].begin(); k != _feature2mef[*j].end(); k++) {
	  const double me = membp[_fb.Feature(*k).label()];
	  const double ee = (_fb.Feature(*k).label() == s.label ? 1.0 : 0);
	  const double grad = (me - ee);
	  _vl[*k] -= eta * grad;
#ifdef SGD_CP
	  apply_l1_penalty(*k, u, _vl, q);
#endif
	}
      }
      // real-valued features
      for (vector<pair<int, double> >::const_iterator j = s.rvfeatures.begin(); j != s.rvfeatures.end(); j++) {
	for (vector<int>::const_iterator k = _feature2mef[j->first].begin(); k != _feature2mef[j->first].end(); k++) {
	  const double me = membp[_fb.Feature(*k).label()];
	  const double ee = (_fb.Feature(*k).label() == s.label ? 1.0 : 0);
	  const double grad = (me - ee) * j->second;
	  _vl[*k] -= eta * grad;
#ifdef SGD_CP
	  apply_l1_penalty(*k, u, _vl, q);
#endif
	}
      }

#ifdef FOLOS_NAIVE
      for (size_t j = 0; j < d; j++) {
	double & x = _vl[j];
	if (x > 0) x = max(0.0, x - eta * l1param);
	else       x = min(0.0, x + eta * l1param);
      }
#endif

    }
    logl /= _vs.size();
    //    fprintf(stderr, "%4d logl = %8.3f acc = %6.4f ", iter, logl, (double)ncorrect / ntotal);

#ifdef FOLOS_LAZY
    if (l1param > 0) {
      for (size_t j = 0; j < d; j++)
	update_folos_lazy(iter_sample, j, _vl, sum_eta, last_updated);
    }
#endif

    double f = logl;
    if (l1param > 0) {
      const double l1 = l1norm(_vl); // this is not accurate when lazy update is used
      //      cerr << "f0 = " <<  update_model_expectation() - l1param * l1 << " ";
      f -= l1param * l1;
      int nonzero = 0;
      for (int j = 0; j < d; j++) if (_vl[j] != 0) nonzero++;
      //      cerr << " f = " << f << " l1 = " << l1 << " nonzero_features = " << nonzero << endl;
    }
    //    fprintf(stderr, "%4d  obj = %7.3f acc = %6.4f", iter+1, f, (double)ncorrect/ntotal);
    //    fprintf(stderr, "%4d  obj = %f", iter+1, f);
    fprintf(stderr, "%3d  obj(err) = %f (%6.4f)", iter+1, f, 1 - (double)ncorrect/ntotal);

    if (_nheldout > 0) {
      double heldout_logl = heldout_likelihood();
      //      fprintf(stderr, "  heldout_logl = %f  acc = %6.4f\n", heldout_logl, 1 - _heldout_error);
      fprintf(stderr, "  heldout_logl(err) = %f (%6.4f)", heldout_logl, _heldout_error);
    }
    fprintf(stderr, "\n");

    
  }

  return 0;
}
