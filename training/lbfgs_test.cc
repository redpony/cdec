#include <cassert>
#include <iostream>
#include <sstream>
#include <cmath>
#include "lbfgs.h"
#include "sparse_vector.h"
#include "fdict.h"

using namespace std;

double TestOptimizer() {
  cerr << "TESTING NON-PERSISTENT OPTIMIZER\n";

  // f(x,y) = 4x1^2 + x1*x2 + x2^2 + x3^2 + 6x3 + 5
  // df/dx1 = 8*x1 + x2
  // df/dx2 = 2*x2 + x1
  // df/dx3 = 2*x3 + 6
  double x[3];
  double g[3];
  scitbx::lbfgs::minimizer<double> opt(3);
  scitbx::lbfgs::traditional_convergence_test<double> converged(3);
  x[0] = 8;
  x[1] = 8;
  x[2] = 8;
  double obj = 0;
  do {
    g[0] = 8 * x[0] + x[1];
    g[1] = 2 * x[1] + x[0];
    g[2] = 2 * x[2] + 6;
    obj = 4 * x[0]*x[0] + x[0] * x[1] + x[1]*x[1] + x[2]*x[2] + 6 * x[2] + 5;
    opt.run(x, obj, g);
    if (!opt.requests_f_and_g()) {
      if (converged(x,g)) break;
      opt.run(x, obj, g);
    }
    cerr << x[0] << " " << x[1] << " " << x[2] << endl;
    cerr << "   obj=" << obj << "\td/dx1=" << g[0] << " d/dx2=" << g[1] << " d/dx3=" << g[2] << endl;
    cerr << opt << endl;
  } while (true);
  return obj;
}

double TestPersistentOptimizer() {
  cerr << "\nTESTING PERSISTENT OPTIMIZER\n";
  // f(x,y) = 4x1^2 + x1*x2 + x2^2 + x3^2 + 6x3 + 5
  // df/dx1 = 8*x1 + x2
  // df/dx2 = 2*x2 + x1
  // df/dx3 = 2*x3 + 6
  double x[3];
  double g[3];
  scitbx::lbfgs::traditional_convergence_test<double> converged(3);
  x[0] = 8;
  x[1] = 8;
  x[2] = 8;
  double obj = 0;
  string state;
  do {
    g[0] = 8 * x[0] + x[1];
    g[1] = 2 * x[1] + x[0];
    g[2] = 2 * x[2] + 6;
    obj = 4 * x[0]*x[0] + x[0] * x[1] + x[1]*x[1] + x[2]*x[2] + 6 * x[2] + 5;

    {
      scitbx::lbfgs::minimizer<double> opt(3);
      if (state.size() > 0) {
        istringstream is(state, ios::binary);
        opt.deserialize(&is);
      }
      opt.run(x, obj, g);
      ostringstream os(ios::binary); opt.serialize(&os); state = os.str();
    }

    cerr << x[0] << " " << x[1] << " " << x[2] << endl;
    cerr << "   obj=" << obj << "\td/dx1=" << g[0] << " d/dx2=" << g[1] << " d/dx3=" << g[2] << endl;
  } while (!converged(x, g));
  return obj;
}

void TestSparseVector() {
  cerr << "Testing SparseVector<double> serialization.\n";
  int f1 = FD::Convert("Feature_1");
  int f2 = FD::Convert("Feature_2");
  FD::Convert("LanguageModel");
  int f4 = FD::Convert("SomeFeature");
  int f5 = FD::Convert("SomeOtherFeature");
  SparseVector<double> g;
  g.set_value(f2, log(0.5));
  g.set_value(f4, log(0.125));
  g.set_value(f1, 0);
  g.set_value(f5, 23.777);
  ostringstream os;
  double iobj = 1.5;
  B64::Encode(iobj, g, &os);
  cerr << iobj << "\t" << g << endl;
  string data = os.str();
  cout << data << endl;
  SparseVector<double> v;
  double obj;
  bool decode_b64 = B64::Decode(&obj, &v, &data[0], data.size());
  cerr << obj << "\t" << v << endl;
  assert(decode_b64);
  assert(obj == iobj);
  assert(g.size() == v.size());
}

int main() {
  double o1 = TestOptimizer();
  double o2 = TestPersistentOptimizer();
  if (fabs(o1 - o2) > 1e-5) {
    cerr << "OPTIMIZERS PERFORMED DIFFERENTLY!\n" << o1 << " vs. " << o2 << endl;
    return 1;
  }
  TestSparseVector();
  cerr << "SUCCESS\n";
  return 0;
}

