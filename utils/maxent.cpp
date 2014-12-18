/*
 * $Id: maxent.cpp,v 1.1.1.1 2007/05/15 08:30:35 kyoshida Exp $
 */

#include "maxent.h"

#include <vector>
#include <iostream>
#include <cmath>
#include <cstdio>

using namespace std;

namespace maxent {
double ME_Model::FunctionGradient(const vector<double>& x,
                                  vector<double>& grad) {
  assert((int)_fb.Size() == x.size());
  for (size_t i = 0; i < x.size(); i++) {
    _vl[i] = x[i];
  }

  double score = update_model_expectation();

  if (_l2reg == 0) {
    for (size_t i = 0; i < x.size(); i++) {
      grad[i] = -(_vee[i] - _vme[i]);
    }
  } else {
    const double c = _l2reg * 2;
    for (size_t i = 0; i < x.size(); i++) {
      grad[i] = -(_vee[i] - _vme[i] - c * _vl[i]);
    }
  }

  return -score;
}

int ME_Model::perform_GIS(int C) {
  cerr << "C = " << C << endl;
  C = 1;
  cerr << "performing AGIS" << endl;
  vector<double> pre_v;
  double pre_logl = -999999;
  for (int iter = 0; iter < 200; iter++) {

    double logl = update_model_expectation();
    fprintf(stderr, "iter = %2d  C = %d  f = %10.7f  train_err = %7.5f", iter,
            C, logl, _train_error);
    if (_heldout.size() > 0) {
      double hlogl = heldout_likelihood();
      fprintf(stderr, "  heldout_logl(err) = %f (%6.4f)", hlogl,
              _heldout_error);
    }
    cerr << endl;

    if (logl < pre_logl) {
      C += 1;
      _vl = pre_v;
      iter--;
      continue;
    }
    if (C > 1 && iter % 10 == 0) C--;

    pre_logl = logl;
    pre_v = _vl;
    for (int i = 0; i < _fb.Size(); i++) {
      double coef = _vee[i] / _vme[i];
      _vl[i] += log(coef) / C;
    }
  }
  cerr << endl;

  return 0;
}

int ME_Model::perform_QUASI_NEWTON() {
  const int dim = _fb.Size();
  vector<double> x0(dim);

  for (int i = 0; i < dim; i++) {
    x0[i] = _vl[i];
  }

  vector<double> x;
  if (_l1reg > 0) {
    cerr << "performing OWLQN" << endl;
    x = perform_OWLQN(x0, _l1reg);
  } else {
    cerr << "performing LBFGS" << endl;
    x = perform_LBFGS(x0);
  }

  for (int i = 0; i < dim; i++) {
    _vl[i] = x[i];
  }

  return 0;
}

int ME_Model::conditional_probability(const Sample& s,
                                      std::vector<double>& membp) const {
  // int num_classes = membp.size();
  double sum = 0;
  int max_label = 0;
  //  double maxp = 0;

  vector<double> powv(_num_classes, 0.0);
  for (vector<int>::const_iterator j = s.positive_features.begin();
       j != s.positive_features.end(); j++) {
    for (vector<int>::const_iterator k = _feature2mef[*j].begin();
         k != _feature2mef[*j].end(); k++) {
      powv[_fb.Feature(*k).label()] += _vl[*k];
    }
  }
  for (vector<pair<int, double> >::const_iterator j = s.rvfeatures.begin();
       j != s.rvfeatures.end(); j++) {
    for (vector<int>::const_iterator k = _feature2mef[j->first].begin();
         k != _feature2mef[j->first].end(); k++) {
      powv[_fb.Feature(*k).label()] += _vl[*k] * j->second;
    }
  }

  std::vector<double>::const_iterator pmax =
      max_element(powv.begin(), powv.end());
  double offset = max(0.0, *pmax - 700);  // to avoid overflow
  for (int label = 0; label < _num_classes; label++) {
    double pow = powv[label] - offset;
    double prod = exp(pow);
    //      cout << pow << " " << prod << ", ";
    //      if (_ref_modelp != NULL) prod *= _train_refpd[n][label];
    if (_ref_modelp != NULL) prod *= s.ref_pd[label];
    assert(prod != 0);
    membp[label] = prod;
    sum += prod;
  }
  for (int label = 0; label < _num_classes; label++) {
    membp[label] /= sum;
    if (membp[label] > membp[max_label]) max_label = label;
  }
  return max_label;
}

int ME_Model::make_feature_bag(const int cutoff) {
  int max_num_features = 0;

// count the occurrences of features
#ifdef USE_HASH_MAP
  typedef std::unordered_map<unsigned int, int> map_type;
#else
  typedef std::map<unsigned int, int> map_type;
#endif
  map_type count;
  if (cutoff > 0) {
    for (std::vector<Sample>::const_iterator i = _vs.begin(); i != _vs.end();
         i++) {
      for (std::vector<int>::const_iterator j = i->positive_features.begin();
           j != i->positive_features.end(); j++) {
        count[ME_Feature(i->label, *j).body()]++;
      }
      for (std::vector<pair<int, double> >::const_iterator j =
               i->rvfeatures.begin();
           j != i->rvfeatures.end(); j++) {
        count[ME_Feature(i->label, j->first).body()]++;
      }
    }
  }

  int n = 0;
  for (std::vector<Sample>::const_iterator i = _vs.begin(); i != _vs.end();
       i++, n++) {
    max_num_features =
        max(max_num_features, (int)(i->positive_features.size()));
    for (std::vector<int>::const_iterator j = i->positive_features.begin();
         j != i->positive_features.end(); j++) {
      const ME_Feature feature(i->label, *j);
      //      if (cutoff > 0 && count[feature.body()] < cutoff) continue;
      if (cutoff > 0 && count[feature.body()] <= cutoff) continue;
      _fb.Put(feature);
      //      cout << i->label << "\t" << *j << "\t" << id << endl;
      //      feature2sample[id].push_back(n);
    }
    for (std::vector<pair<int, double> >::const_iterator j =
             i->rvfeatures.begin();
         j != i->rvfeatures.end(); j++) {
      const ME_Feature feature(i->label, j->first);
      //      if (cutoff > 0 && count[feature.body()] < cutoff) continue;
      if (cutoff > 0 && count[feature.body()] <= cutoff) continue;
      _fb.Put(feature);
    }
  }
  count.clear();

  //  cerr << "num_classes = " << _num_classes << endl;
  //  cerr << "max_num_features = " << max_num_features << endl;

  init_feature2mef();

  return max_num_features;
}

double ME_Model::heldout_likelihood() {
  double logl = 0;
  int ncorrect = 0;
  for (std::vector<Sample>::const_iterator i = _heldout.begin();
       i != _heldout.end(); i++) {
    vector<double> membp(_num_classes);
    int l = classify(*i, membp);
    logl += log(membp[i->label]);
    if (l == i->label) ncorrect++;
  }
  _heldout_error = 1 - (double)ncorrect / _heldout.size();

  return logl /= _heldout.size();
}

double ME_Model::update_model_expectation() {
  double logl = 0;
  int ncorrect = 0;

  _vme.resize(_fb.Size());
  for (int i = 0; i < _fb.Size(); i++) _vme[i] = 0;

  int n = 0;
  for (vector<Sample>::const_iterator i = _vs.begin(); i != _vs.end();
       i++, n++) {
    vector<double> membp(_num_classes);
    int max_label = conditional_probability(*i, membp);

    logl += log(membp[i->label]);
    //    cout << membp[*i] << " " << logl << " ";
    if (max_label == i->label) ncorrect++;

    // model_expectation
    for (vector<int>::const_iterator j = i->positive_features.begin();
         j != i->positive_features.end(); j++) {
      for (vector<int>::const_iterator k = _feature2mef[*j].begin();
           k != _feature2mef[*j].end(); k++) {
        _vme[*k] += membp[_fb.Feature(*k).label()];
      }
    }
    for (vector<pair<int, double> >::const_iterator j = i->rvfeatures.begin();
         j != i->rvfeatures.end(); j++) {
      for (vector<int>::const_iterator k = _feature2mef[j->first].begin();
           k != _feature2mef[j->first].end(); k++) {
        _vme[*k] += membp[_fb.Feature(*k).label()] * j->second;
      }
    }
  }

  for (int i = 0; i < _fb.Size(); i++) {
    _vme[i] /= _vs.size();
  }

  _train_error = 1 - (double)ncorrect / _vs.size();

  logl /= _vs.size();

  if (_l2reg > 0) {
    const double c = _l2reg;
    for (int i = 0; i < _fb.Size(); i++) {
      logl -= _vl[i] * _vl[i] * c;
    }
  }

  // logl /= _vs.size();

  //  fprintf(stderr, "iter =%3d  logl = %10.7f  train_acc = %7.5f\n", iter,
  // logl, (double)ncorrect/train.size());
  //  fprintf(stderr, "logl = %10.7f  train_acc = %7.5f\n", logl,
  // (double)ncorrect/_train.size());

  return logl;
}

int ME_Model::train(const vector<ME_Sample>& vms) {
  _vs.clear();
  for (vector<ME_Sample>::const_iterator i = vms.begin(); i != vms.end(); i++) {
    add_training_sample(*i);
  }

  return train();
}

void ME_Model::add_training_sample(const ME_Sample& mes) {
  Sample s;
  s.label = _label_bag.Put(mes.label);
  if (s.label > ME_Feature::MAX_LABEL_TYPES) {
    cerr << "error: too many types of labels." << endl;
    exit(1);
  }
  for (vector<string>::const_iterator j = mes.features.begin();
       j != mes.features.end(); j++) {
    s.positive_features.push_back(_featurename_bag.Put(*j));
  }
  for (vector<pair<string, double> >::const_iterator j = mes.rvfeatures.begin();
       j != mes.rvfeatures.end(); j++) {
    s.rvfeatures.push_back(
        pair<int, double>(_featurename_bag.Put(j->first), j->second));
  }
  if (_ref_modelp != NULL) {
    ME_Sample tmp = mes;
    ;
    s.ref_pd = _ref_modelp->classify(tmp);
  }
  //  cout << s.label << "\t";
  //  for (vector<int>::const_iterator j = s.positive_features.begin(); j !=
  // s.positive_features.end(); j++){
  //    cout << *j << " ";
  //  }
  //  cout << endl;

  _vs.push_back(s);
}

int ME_Model::train() {
  if (_l1reg > 0 && _l2reg > 0) {
    cerr << "error: L1 and L2 regularizers cannot be used simultaneously."
         << endl;
    return 0;
  }
  if (_vs.size() == 0) {
    cerr << "error: no training data." << endl;
    return 0;
  }
  if (_nheldout >= (int)_vs.size()) {
    cerr << "error: too much heldout data. no training data is available."
         << endl;
    return 0;
  }
  //  if (_nheldout > 0) random_shuffle(_vs.begin(), _vs.end());

  int max_label = 0;
  for (std::vector<Sample>::const_iterator i = _vs.begin(); i != _vs.end();
       i++) {
    max_label = max(max_label, i->label);
  }
  _num_classes = max_label + 1;
  if (_num_classes != _label_bag.Size()) {
    cerr << "warning: _num_class != _label_bag.Size()" << endl;
  }

  if (_ref_modelp != NULL) {
    cerr << "setting reference distribution...";
    for (int i = 0; i < _ref_modelp->num_classes(); i++) {
      _label_bag.Put(_ref_modelp->get_class_label(i));
    }
    _num_classes = _label_bag.Size();
    for (vector<Sample>::iterator i = _vs.begin(); i != _vs.end(); i++) {
      set_ref_dist(*i);
    }
    cerr << "done" << endl;
  }

  for (int i = 0; i < _nheldout; i++) {
    _heldout.push_back(_vs.back());
    _vs.pop_back();
  }

  sort(_vs.begin(), _vs.end());

  int cutoff = 0;
  if (cutoff > 0) cerr << "cutoff threshold = " << cutoff << endl;
  if (_l1reg > 0) cerr << "L1 regularizer = " << _l1reg << endl;
  if (_l2reg > 0) cerr << "L2 regularizer = " << _l2reg << endl;

  // normalize
  _l1reg /= _vs.size();
  _l2reg /= _vs.size();

  cerr << "preparing for estimation...";
  make_feature_bag(cutoff);
  //  _vs.clear();
  cerr << "done" << endl;
  cerr << "number of samples = " << _vs.size() << endl;
  cerr << "number of features = " << _fb.Size() << endl;

  cerr << "calculating empirical expectation...";
  _vee.resize(_fb.Size());
  for (int i = 0; i < _fb.Size(); i++) {
    _vee[i] = 0;
  }
  for (int n = 0; n < (int)_vs.size(); n++) {
    const Sample* i = &_vs[n];
    for (vector<int>::const_iterator j = i->positive_features.begin();
         j != i->positive_features.end(); j++) {
      for (vector<int>::const_iterator k = _feature2mef[*j].begin();
           k != _feature2mef[*j].end(); k++) {
        if (_fb.Feature(*k).label() == i->label) _vee[*k] += 1.0;
      }
    }

    for (vector<pair<int, double> >::const_iterator j = i->rvfeatures.begin();
         j != i->rvfeatures.end(); j++) {
      for (vector<int>::const_iterator k = _feature2mef[j->first].begin();
           k != _feature2mef[j->first].end(); k++) {
        if (_fb.Feature(*k).label() == i->label) _vee[*k] += j->second;
      }
    }
  }
  for (int i = 0; i < _fb.Size(); i++) {
    _vee[i] /= _vs.size();
  }
  cerr << "done" << endl;

  _vl.resize(_fb.Size());
  for (int i = 0; i < _fb.Size(); i++) _vl[i] = 0.0;

  if (_optimization_method == SGD) {
    perform_SGD();
  } else {
    perform_QUASI_NEWTON();
  }

  int num_active = 0;
  for (int i = 0; i < _fb.Size(); i++) {
    if (_vl[i] != 0) num_active++;
  }
  cerr << "number of active features = " << num_active << endl;

  return 0;
}

void ME_Model::get_features(list<pair<pair<string, string>, double> >& fl) {
  fl.clear();
  //  for (int i = 0; i < _fb.Size(); i++) {
  //    ME_Feature f = _fb.Feature(i);
  //    fl.push_back( make_pair(make_pair(_label_bag.Str(f.label()),
  // _featurename_bag.Str(f.feature())), _vl[i]));
  //  }
  for (MiniStringBag::map_type::const_iterator i = _featurename_bag.begin();
       i != _featurename_bag.end(); i++) {
    for (int j = 0; j < _label_bag.Size(); j++) {
      string label = _label_bag.Str(j);
      string history = i->first;
      int id = _fb.Id(ME_Feature(j, i->second));
      if (id < 0) continue;
      fl.push_back(make_pair(make_pair(label, history), _vl[id]));
    }
  }
}

void ME_Model::clear() {
  _vl.clear();
  _label_bag.Clear();
  _featurename_bag.Clear();
  _fb.Clear();
  _feature2mef.clear();
  _vee.clear();
  _vme.clear();
  _vs.clear();
  _heldout.clear();
}

bool ME_Model::load_from_file(const string& filename) {
  FILE* fp = fopen(filename.c_str(), "r");
  if (!fp) {
    cerr << "error: cannot open " << filename << "!" << endl;
    return false;
  }

  _vl.clear();
  _label_bag.Clear();
  _featurename_bag.Clear();
  _fb.Clear();
  char buf[1024];
  while (fgets(buf, 1024, fp)) {
    string line(buf);
    string::size_type t1 = line.find_first_of('\t');
    string::size_type t2 = line.find_last_of('\t');
    string classname = line.substr(0, t1);
    string featurename = line.substr(t1 + 1, t2 - (t1 + 1));
    float lambda;
    string w = line.substr(t2 + 1);
    sscanf(w.c_str(), "%f", &lambda);

    int label = _label_bag.Put(classname);
    int feature = _featurename_bag.Put(featurename);
    _fb.Put(ME_Feature(label, feature));
    _vl.push_back(lambda);
  }

  _num_classes = _label_bag.Size();

  init_feature2mef();

  fclose(fp);

  return true;
}

void ME_Model::init_feature2mef() {
  _feature2mef.clear();
  for (int i = 0; i < _featurename_bag.Size(); i++) {
    vector<int> vi;
    for (int k = 0; k < _num_classes; k++) {
      int id = _fb.Id(ME_Feature(k, i));
      if (id >= 0) vi.push_back(id);
    }
    _feature2mef.push_back(vi);
  }
}

bool ME_Model::load_from_array(const ME_Model_Data data[]) {
  _vl.clear();
  for (int i = 0;; i++) {
    if (string(data[i].label) == "///") break;
    int label = _label_bag.Put(data[i].label);
    int feature = _featurename_bag.Put(data[i].feature);
    _fb.Put(ME_Feature(label, feature));
    _vl.push_back(data[i].weight);
  }
  _num_classes = _label_bag.Size();

  init_feature2mef();

  return true;
}

bool ME_Model::save_to_file(const string& filename, const double th) const {
  FILE* fp = fopen(filename.c_str(), "w");
  if (!fp) {
    cerr << "error: cannot open " << filename << "!" << endl;
    return false;
  }

  //  for (int i = 0; i < _fb.Size(); i++) {
  //    if (_vl[i] == 0) continue; // ignore zero-weight features
  //    ME_Feature f = _fb.Feature(i);
  //    fprintf(fp, "%s\t%s\t%f\n", _label_bag.Str(f.label()).c_str(),
  // _featurename_bag.Str(f.feature()).c_str(), _vl[i]);
  //  }
  for (MiniStringBag::map_type::const_iterator i = _featurename_bag.begin();
       i != _featurename_bag.end(); i++) {
    for (int j = 0; j < _label_bag.Size(); j++) {
      string label = _label_bag.Str(j);
      string history = i->first;
      int id = _fb.Id(ME_Feature(j, i->second));
      if (id < 0) continue;
      if (_vl[id] == 0) continue;        // ignore zero-weight features
      if (fabs(_vl[id]) < th) continue;  // cut off low-weight features
      fprintf(fp, "%s\t%s\t%f\n", label.c_str(), history.c_str(), _vl[id]);
    }
  }

  fclose(fp);

  return true;
}

void ME_Model::set_ref_dist(Sample& s) const {
  vector<double> v0 = s.ref_pd;
  vector<double> v(_num_classes);
  for (unsigned int i = 0; i < v.size(); i++) {
    v[i] = 0;
    string label = get_class_label(i);
    int id_ref = _ref_modelp->get_class_id(label);
    if (id_ref != -1) {
      v[i] = v0[id_ref];
    }
    if (v[i] == 0) v[i] = 0.001;  // to avoid -inf logl
  }
  s.ref_pd = v;
}

int ME_Model::classify(const Sample& nbs, vector<double>& membp) const {
  //  vector<double> membp(_num_classes);
  assert(_num_classes == (int)membp.size());
  conditional_probability(nbs, membp);
  int max_label = 0;
  double max = 0.0;
  for (int i = 0; i < (int)membp.size(); i++) {
    //    cout << membp[i] << " ";
    if (membp[i] > max) {
      max_label = i;
      max = membp[i];
    }
  }
  //  cout << endl;
  return max_label;
}

vector<double> ME_Model::classify(ME_Sample& mes) const {
  Sample s;
  for (vector<string>::const_iterator j = mes.features.begin();
       j != mes.features.end(); j++) {
    int id = _featurename_bag.Id(*j);
    if (id >= 0) s.positive_features.push_back(id);
  }
  for (vector<pair<string, double> >::const_iterator j = mes.rvfeatures.begin();
       j != mes.rvfeatures.end(); j++) {
    int id = _featurename_bag.Id(j->first);
    if (id >= 0) {
      s.rvfeatures.push_back(pair<int, double>(id, j->second));
    }
  }
  if (_ref_modelp != NULL) {
    s.ref_pd = _ref_modelp->classify(mes);
    set_ref_dist(s);
  }

  vector<double> vp(_num_classes);
  int label = classify(s, vp);
  mes.label = get_class_label(label);
  return vp;
}

// template<class FuncGrad>
// std::vector<double>
// perform_LBFGS(FuncGrad func_grad, const std::vector<double> & x0);

std::vector<double> perform_LBFGS(
    double (*func_grad)(const std::vector<double> &, std::vector<double> &),
    const std::vector<double> &x0);

std::vector<double> perform_OWLQN(
    double (*func_grad)(const std::vector<double> &, std::vector<double> &),
    const std::vector<double> &x0, const double C);

const int LBFGS_M = 10;

const static int M = LBFGS_M;
const static double LINE_SEARCH_ALPHA = 0.1;
const static double LINE_SEARCH_BETA = 0.5;

// stopping criteria
int LBFGS_MAX_ITER = 300;
const static double MIN_GRAD_NORM = 0.0001;

// LBFGS

double ME_Model::backtracking_line_search(const Vec& x0, const Vec& grad0,
                                          const double f0, const Vec& dx,
                                          Vec& x, Vec& grad1) {
  double t = 1.0 / LINE_SEARCH_BETA;

  double f;
  do {
    t *= LINE_SEARCH_BETA;
    x = x0 + t * dx;
    f = FunctionGradient(x.STLVec(), grad1.STLVec());
    //        cout << "*";
  } while (f > f0 + LINE_SEARCH_ALPHA * t * dot_product(dx, grad0));

  return f;
}

//
// Jorge Nocedal, "Updating Quasi-Newton Matrices With Limited Storage",
// Mathematics of Computation, Vol. 35, No. 151, pp. 773-782, 1980.
//
Vec approximate_Hg(const int iter, const Vec& grad, const Vec s[],
                   const Vec y[], const double z[]) {
  int offset, bound;
  if (iter <= M) {
    offset = 0;
    bound = iter;
  } else {
    offset = iter - M;
    bound = M;
  }

  Vec q = grad;
  double alpha[M], beta[M];
  for (int i = bound - 1; i >= 0; i--) {
    const int j = (i + offset) % M;
    alpha[i] = z[j] * dot_product(s[j], q);
    q += -alpha[i] * y[j];
  }
  if (iter > 0) {
    const int j = (iter - 1) % M;
    const double gamma = ((1.0 / z[j]) / dot_product(y[j], y[j]));
    //    static double gamma;
    //    if (gamma == 0) gamma = ((1.0 / z[j]) / dot_product(y[j], y[j]));
    q *= gamma;
  }
  for (int i = 0; i <= bound - 1; i++) {
    const int j = (i + offset) % M;
    beta[i] = z[j] * dot_product(y[j], q);
    q += s[j] * (alpha[i] - beta[i]);
  }

  return q;
}

vector<double> ME_Model::perform_LBFGS(const vector<double>& x0) {
  const size_t dim = x0.size();
  Vec x = x0;

  Vec grad(dim), dx(dim);
  double f = FunctionGradient(x.STLVec(), grad.STLVec());

  Vec s[M], y[M];
  double z[M];  // rho

  for (int iter = 0; iter < LBFGS_MAX_ITER; iter++) {

    fprintf(stderr, "%3d  obj(err) = %f (%6.4f)", iter + 1, -f, _train_error);
    if (_nheldout > 0) {
      const double heldout_logl = heldout_likelihood();
      fprintf(stderr, "  heldout_logl(err) = %f (%6.4f)", heldout_logl,
              _heldout_error);
    }
    fprintf(stderr, "\n");

    if (sqrt(dot_product(grad, grad)) < MIN_GRAD_NORM) break;

    dx = -1 * approximate_Hg(iter, grad, s, y, z);

    Vec x1(dim), grad1(dim);
    f = backtracking_line_search(x, grad, f, dx, x1, grad1);

    s[iter % M] = x1 - x;
    y[iter % M] = grad1 - grad;
    z[iter % M] = 1.0 / dot_product(y[iter % M], s[iter % M]);
    x = x1;
    grad = grad1;
  }

  return x.STLVec();
}

// OWLQN

// stopping criteria
int OWLQN_MAX_ITER = 300;

Vec approximate_Hg(const int iter, const Vec& grad, const Vec s[],
                   const Vec y[], const double z[]);

inline int sign(double x) {
  if (x > 0) return 1;
  if (x < 0) return -1;
  return 0;
};

static Vec pseudo_gradient(const Vec& x, const Vec& grad0, const double C) {
  Vec grad = grad0;
  for (size_t i = 0; i < x.Size(); i++) {
    if (x[i] != 0) {
      grad[i] += C * sign(x[i]);
      continue;
    }
    const double gm = grad0[i] - C;
    if (gm > 0) {
      grad[i] = gm;
      continue;
    }
    const double gp = grad0[i] + C;
    if (gp < 0) {
      grad[i] = gp;
      continue;
    }
    grad[i] = 0;
  }

  return grad;
}

double ME_Model::regularized_func_grad(const double C, const Vec& x,
                                       Vec& grad) {
  double f = FunctionGradient(x.STLVec(), grad.STLVec());
  for (size_t i = 0; i < x.Size(); i++) {
    f += C * fabs(x[i]);
  }

  return f;
}

double ME_Model::constrained_line_search(double C, const Vec& x0,
                                         const Vec& grad0, const double f0,
                                         const Vec& dx, Vec& x, Vec& grad1) {
  // compute the orthant to explore
  Vec orthant = x0;
  for (size_t i = 0; i < orthant.Size(); i++) {
    if (orthant[i] == 0) orthant[i] = -grad0[i];
  }

  double t = 1.0 / LINE_SEARCH_BETA;

  double f;
  do {
    t *= LINE_SEARCH_BETA;
    x = x0 + t * dx;
    x.Project(orthant);
    //    for (size_t i = 0; i < x.Size(); i++) {
    //      if (x0[i] != 0 && sign(x[i]) != sign(x0[i])) x[i] = 0;
    //    }

    f = regularized_func_grad(C, x, grad1);
    //        cout << "*";
  } while (f > f0 + LINE_SEARCH_ALPHA * dot_product(x - x0, grad0));

  return f;
}

vector<double> ME_Model::perform_OWLQN(const vector<double>& x0,
                                       const double C) {
  const size_t dim = x0.size();
  Vec x = x0;

  Vec grad(dim), dx(dim);
  double f = regularized_func_grad(C, x, grad);

  Vec s[M], y[M];
  double z[M];  // rho

  for (int iter = 0; iter < OWLQN_MAX_ITER; iter++) {
    Vec pg = pseudo_gradient(x, grad, C);

    fprintf(stderr, "%3d  obj(err) = %f (%6.4f)", iter + 1, -f, _train_error);
    if (_nheldout > 0) {
      const double heldout_logl = heldout_likelihood();
      fprintf(stderr, "  heldout_logl(err) = %f (%6.4f)", heldout_logl,
              _heldout_error);
    }
    fprintf(stderr, "\n");

    if (sqrt(dot_product(pg, pg)) < MIN_GRAD_NORM) break;

    dx = -1 * approximate_Hg(iter, pg, s, y, z);
    if (dot_product(dx, pg) >= 0) dx.Project(-1 * pg);

    Vec x1(dim), grad1(dim);
    f = constrained_line_search(C, x, pg, f, dx, x1, grad1);

    s[iter % M] = x1 - x;
    y[iter % M] = grad1 - grad;
    z[iter % M] = 1.0 / dot_product(y[iter % M], s[iter % M]);

    x = x1;
    grad = grad1;
  }

  return x.STLVec();
}

// SGD

// const double SGD_ETA0 = 1;
// const double SGD_ITER = 30;
// const double SGD_ALPHA = 0.85;

//#define FOLOS_NAIVE
//#define FOLOS_LAZY
#define SGD_CP

inline void apply_l1_penalty(const int i, const double u, vector<double>& _vl,
                             vector<double>& q) {
  double& w = _vl[i];
  const double z = w;
  double& qi = q[i];
  if (w > 0) {
    w = max(0.0, w - (u + qi));
  } else if (w < 0) {
    w = min(0.0, w + (u - qi));
  }
  qi += w - z;
}

static double l1norm(const vector<double>& v) {
  double sum = 0;
  for (size_t i = 0; i < v.size(); i++) sum += abs(v[i]);
  return sum;
}

inline void update_folos_lazy(const int iter_sample, const int k,
                              vector<double>& _vl,
                              const vector<double>& sum_eta,
                              vector<int>& last_updated) {
  const double penalty = sum_eta[iter_sample] - sum_eta[last_updated[k]];
  double& x = _vl[k];
  if (x > 0)
    x = max(0.0, x - penalty);
  else
    x = min(0.0, x + penalty);
  last_updated[k] = iter_sample;
}

int ME_Model::perform_SGD() {
  if (_l2reg > 0) {
    cerr << "error: L2 regularization is currently not supported in SGD mode."
         << endl;
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
      const Sample& s = _vs[ri[i]];

#ifdef FOLOS_LAZY
      for (vector<int>::const_iterator j = s.positive_features.begin();
           j != s.positive_features.end(); j++) {
        for (vector<int>::const_iterator k = _feature2mef[*j].begin();
             k != _feature2mef[*j].end(); k++) {
          update_folos_lazy(iter_sample, *k, _vl, sum_eta, last_updated);
        }
      }
#endif

      vector<double> membp(_num_classes);
      const int max_label = conditional_probability(s, membp);

      const double eta =
          eta0 * pow(SGD_ALPHA,
                     (double)iter_sample / _vs.size());  // exponential decay
      //      const double eta = eta0 / (1.0 + (double)iter_sample /
      // _vs.size());

      //      if (iter_sample % _vs.size() == 0) cerr << "eta = " << eta <<
      // endl;
      u += eta * l1param;

      sum_eta.push_back(sum_eta.back() + eta * l1param);

      logl += log(membp[s.label]);
      if (max_label == s.label) ncorrect++;

      // binary features
      for (vector<int>::const_iterator j = s.positive_features.begin();
           j != s.positive_features.end(); j++) {
        for (vector<int>::const_iterator k = _feature2mef[*j].begin();
             k != _feature2mef[*j].end(); k++) {
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
      for (vector<pair<int, double> >::const_iterator j = s.rvfeatures.begin();
           j != s.rvfeatures.end(); j++) {
        for (vector<int>::const_iterator k = _feature2mef[j->first].begin();
             k != _feature2mef[j->first].end(); k++) {
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
        double& x = _vl[j];
        if (x > 0)
          x = max(0.0, x - eta * l1param);
        else
          x = min(0.0, x + eta * l1param);
      }
#endif
    }
    logl /= _vs.size();
//    fprintf(stderr, "%4d logl = %8.3f acc = %6.4f ", iter, logl,
// (double)ncorrect / ntotal);

#ifdef FOLOS_LAZY
    if (l1param > 0) {
      for (size_t j = 0; j < d; j++)
        update_folos_lazy(iter_sample, j, _vl, sum_eta, last_updated);
    }
#endif

    double f = logl;
    if (l1param > 0) {
      const double l1 =
          l1norm(_vl);  // this is not accurate when lazy update is used
      //      cerr << "f0 = " <<  update_model_expectation() - l1param * l1 << "
      // ";
      f -= l1param * l1;
      int nonzero = 0;
      for (int j = 0; j < d; j++)
        if (_vl[j] != 0) nonzero++;
      //      cerr << " f = " << f << " l1 = " << l1 << " nonzero_features = "
      // << nonzero << endl;
    }
    //    fprintf(stderr, "%4d  obj = %7.3f acc = %6.4f", iter+1, f,
    // (double)ncorrect/ntotal);
    //    fprintf(stderr, "%4d  obj = %f", iter+1, f);
    fprintf(stderr, "%3d  obj(err) = %f (%6.4f)", iter + 1, f,
            1 - (double)ncorrect / ntotal);

    if (_nheldout > 0) {
      double heldout_logl = heldout_likelihood();
      //      fprintf(stderr, "  heldout_logl = %f  acc = %6.4f\n",
      // heldout_logl, 1 - _heldout_error);
      fprintf(stderr, "  heldout_logl(err) = %f (%6.4f)", heldout_logl,
              _heldout_error);
    }
    fprintf(stderr, "\n");
  }

  return 0;
}

}  // namespace maxent

/*
 * $Log: maxent.cpp,v $
 * Revision 1.1.1.1  2007/05/15 08:30:35  kyoshida
 * stepp tagger, by Okanohara and Tsuruoka
 *
 * Revision 1.28  2006/08/21 17:30:38  tsuruoka
 * use MAX_LABEL_TYPES
 *
 * Revision 1.27  2006/07/25 13:19:53  tsuruoka
 * sort _vs[]
 *
 * Revision 1.26  2006/07/18 11:13:15  tsuruoka
 * modify comments
 *
 * Revision 1.25  2006/07/18 10:02:15  tsuruoka
 * remove sample2feature[]
 * speed up conditional_probability()
 *
 * Revision 1.24  2006/07/18 05:10:51  tsuruoka
 * add ref_dist
 *
 * Revision 1.23  2005/12/24 07:05:32  tsuruoka
 * modify conditional_probability() to avoid overflow
 *
 * Revision 1.22  2005/12/24 07:01:25  tsuruoka
 * add cutoff for real-valued features
 *
 * Revision 1.21  2005/12/23 10:33:02  tsuruoka
 * support real-valued features
 *
 * Revision 1.20  2005/12/23 09:15:29  tsuruoka
 * modify _train to reduce memory consumption
 *
 * Revision 1.19  2005/10/28 13:10:14  tsuruoka
 * fix for overflow (thanks to Ming Li)
 *
 * Revision 1.18  2005/10/28 13:03:07  tsuruoka
 * add progress_bar
 *
 * Revision 1.17  2005/09/12 13:51:16  tsuruoka
 * Sample: list -> vector
 *
 * Revision 1.16  2005/09/12 13:27:10  tsuruoka
 * add add_training_sample()
 *
 * Revision 1.15  2005/04/27 11:22:27  tsuruoka
 * bugfix
 * ME_Sample: list -> vector
 *
 * Revision 1.14  2005/04/27 10:00:42  tsuruoka
 * remove tmpfb
 *
 * Revision 1.13  2005/04/26 14:25:53  tsuruoka
 * add MiniStringBag, USE_HASH_MAP
 *
 * Revision 1.12  2005/02/11 10:20:08  tsuruoka
 * modify cutoff
 *
 * Revision 1.11  2004/10/04 05:50:25  tsuruoka
 * add Clear()
 *
 * Revision 1.10  2004/08/26 16:52:26  tsuruoka
 * fix load_from_file()
 *
 * Revision 1.9  2004/08/09 12:27:21  tsuruoka
 * change messages
 *
 * Revision 1.8  2004/08/04 13:55:18  tsuruoka
 * modify _sample2feature
 *
 * Revision 1.7  2004/07/28 13:42:58  tsuruoka
 * add AGIS
 *
 * Revision 1.6  2004/07/28 05:54:13  tsuruoka
 * get_class_name() -> get_class_label()
 * ME_Feature: bugfix
 *
 * Revision 1.5  2004/07/27 16:58:47  tsuruoka
 * modify the interface of classify()
 *
 * Revision 1.4  2004/07/26 17:23:46  tsuruoka
 * _sample2feature: list -> vector
 *
 * Revision 1.3  2004/07/26 15:49:23  tsuruoka
 * modify ME_Feature
 *
 * Revision 1.2  2004/07/26 13:52:18  tsuruoka
 * modify cutoff
 *
 * Revision 1.1  2004/07/26 13:10:55  tsuruoka
 * add files
 *
 * Revision 1.20  2004/07/22 08:34:45  tsuruoka
 * modify _sample2feature[]
 *
 * Revision 1.19  2004/07/21 16:33:01  tsuruoka
 * remove some comments
 *
 */
