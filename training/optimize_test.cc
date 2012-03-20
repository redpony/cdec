#include <cassert>
#include <iostream>
#include <sstream>
#include <boost/program_options/variables_map.hpp>
#include "optimize.h"
#include "online_optimizer.h"
#include "sparse_vector.h"
#include "fdict.h"

using namespace std;

double TestOptimizer(BatchOptimizer* opt) {
  cerr << "TESTING NON-PERSISTENT OPTIMIZER\n";

  // f(x,y) = 4x1^2 + x1*x2 + x2^2 + x3^2 + 6x3 + 5
  // df/dx1 = 8*x1 + x2
  // df/dx2 = 2*x2 + x1
  // df/dx3 = 2*x3 + 6
  vector<double> x(3);
  vector<double> g(3);
  x[0] = 8;
  x[1] = 8;
  x[2] = 8;
  double obj = 0;
  do {
    g[0] = 8 * x[0] + x[1];
    g[1] = 2 * x[1] + x[0];
    g[2] = 2 * x[2] + 6;
    obj = 4 * x[0]*x[0] + x[0] * x[1] + x[1]*x[1] + x[2]*x[2] + 6 * x[2] + 5;
    opt->Optimize(obj, g, &x);

    cerr << x[0] << " " << x[1] << " " << x[2] << endl;
    cerr << "   obj=" << obj << "\td/dx1=" << g[0] << " d/dx2=" << g[1] << " d/dx3=" << g[2] << endl;
  } while (!opt->HasConverged());
  return obj;
}

double TestPersistentOptimizer(BatchOptimizer* opt) {
  cerr << "\nTESTING PERSISTENT OPTIMIZER\n";
  // f(x,y) = 4x1^2 + x1*x2 + x2^2 + x3^2 + 6x3 + 5
  // df/dx1 = 8*x1 + x2
  // df/dx2 = 2*x2 + x1
  // df/dx3 = 2*x3 + 6
  vector<double> x(3);
  vector<double> g(3);
  x[0] = 8;
  x[1] = 8;
  x[2] = 8;
  double obj = 0;
  string state;
  bool converged = false;
  while (!converged) {
    g[0] = 8 * x[0] + x[1];
    g[1] = 2 * x[1] + x[0];
    g[2] = 2 * x[2] + 6;
    obj = 4 * x[0]*x[0] + x[0] * x[1] + x[1]*x[1] + x[2]*x[2] + 6 * x[2] + 5;

    {
      if (state.size() > 0) {
        istringstream is(state, ios::binary);
        opt->Load(&is);
      }
      opt->Optimize(obj, g, &x);
      ostringstream os(ios::binary); opt->Save(&os); state = os.str();

    }

    cerr << x[0] << " " << x[1] << " " << x[2] << endl;
    cerr << "   obj=" << obj << "\td/dx1=" << g[0] << " d/dx2=" << g[1] << " d/dx3=" << g[2] << endl;
    converged = opt->HasConverged();
    if (!converged) {
      // now screw up the state (should be undone by Load)
      obj += 2.0;
      g[1] = -g[2];
      vector<double> x2 = x;
      try {
        opt->Optimize(obj, g, &x2);
      } catch (...) { }
    }
  }
  return obj;
}

template <class O>
void TestOptimizerVariants(int num_vars) {
  O oa(num_vars);
  cerr << "-------------------------------------------------------------------------\n";
  cerr << "TESTING: " << oa.Name() << endl;
  double o1 = TestOptimizer(&oa);
  O ob(num_vars);
  double o2 = TestPersistentOptimizer(&ob);
  if (o1 != o2) {
    cerr << oa.Name() << " VARIANTS PERFORMED DIFFERENTLY!\n" << o1 << " vs. " << o2 << endl;
    exit(1);
  }
  cerr << oa.Name() << " SUCCESS\n";
}

using namespace std::tr1;

void TestOnline() {
  size_t N = 20;
  double C = 1.0;
  double eta0 = 0.2;
  std::tr1::shared_ptr<LearningRateSchedule> r(new ExponentialDecayLearningRate(N, eta0, 0.85));
  //shared_ptr<LearningRateSchedule> r(new StandardLearningRate(N, eta0));
  CumulativeL1OnlineOptimizer opt(r, N, C, std::vector<int>());
  assert(r->eta(10) < r->eta(1));
}

int main() {
  int n = 3;
  TestOptimizerVariants<LBFGSOptimizer>(n);
  TestOptimizerVariants<RPropOptimizer>(n);
  TestOnline();
  return 0;
}

