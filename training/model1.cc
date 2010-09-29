#include <iostream>
#include <cmath>

#include "lattice.h"
#include "stringlib.h"
#include "filelib.h"
#include "ttables.h"
#include "tdict.h"

using namespace std;

int main(int argc, char** argv) {
  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " corpus.fr-en\n";
    return 1;
  }
  const int ITERATIONS = 5;
  const double BEAM_THRESHOLD = 0.0001;
  TTable tt;
  const WordID kNULL = TD::Convert("<eps>");
  bool use_null = true;
  TTable::Word2Word2Double was_viterbi;
  for (int iter = 0; iter < ITERATIONS; ++iter) {
    const bool final_iteration = (iter == (ITERATIONS - 1));
    cerr << "ITERATION " << (iter + 1) << (final_iteration ? " (FINAL)" : "") << endl;
    ReadFile rf(argv[1]);
    istream& in = *rf.stream();
    double likelihood = 0;
    double denom = 0.0;
    int lc = 0;
    bool flag = false;
    while(true) {
      string line;
      getline(in, line);
      if (!in) break;
      ++lc;
      if (lc % 1000 == 0) { cerr << '.'; flag = true; }
      if (lc %50000 == 0) { cerr << " [" << lc << "]\n" << flush; flag = false; }
      string ssrc, strg;
      ParseTranslatorInput(line, &ssrc, &strg);
      Lattice src, trg;
      LatticeTools::ConvertTextToLattice(ssrc, &src);
      LatticeTools::ConvertTextToLattice(strg, &trg);
      assert(src.size() > 0);
      assert(trg.size() > 0);
      denom += 1.0;
      vector<double> probs(src.size() + 1);
      for (int j = 0; j < trg.size(); ++j) {
        const WordID& f_j = trg[j][0].label;
        double sum = 0;
        if (use_null) {
          probs[0] = tt.prob(kNULL, f_j);
          sum += probs[0];
        }
        for (int i = 1; i <= src.size(); ++i) {
          probs[i] = tt.prob(src[i-1][0].label, f_j);
          sum += probs[i];
        }
        if (final_iteration) {
          WordID max_i = 0;
          double max_p = -1;
          if (use_null) {
            max_i = kNULL;
            max_p = probs[0];
          }
          for (int i = 1; i <= src.size(); ++i) {
            if (probs[i] > max_p) {
              max_p = probs[i];
              max_i = src[i-1][0].label;
            }
          }
          was_viterbi[max_i][f_j] = 1.0;
        } else {
          if (use_null)
            tt.Increment(kNULL, f_j, probs[0] / sum);
          for (int i = 1; i <= src.size(); ++i)
            tt.Increment(src[i-1][0].label, f_j, probs[i] / sum);
        }
        likelihood += log(sum);
      }
    }
    if (flag) { cerr << endl; }
    cerr << "  log likelihood: " << likelihood << endl;
    cerr << "    cross entopy: " << (-likelihood / denom) << endl;
    cerr << "      perplexity: " << pow(2.0, -likelihood / denom) << endl;
    if (!final_iteration) tt.Normalize();
  }
  for (TTable::Word2Word2Double::iterator ei = tt.ttable.begin(); ei != tt.ttable.end(); ++ei) {
    const TTable::Word2Double& cpd = ei->second;
    const TTable::Word2Double& vit = was_viterbi[ei->first];
    const string& esym = TD::Convert(ei->first);
    double max_p = -1;
    for (TTable::Word2Double::const_iterator fi = cpd.begin(); fi != cpd.end(); ++fi)
      if (fi->second > max_p) max_p = fi->second;
    const double threshold = max_p * BEAM_THRESHOLD;
    for (TTable::Word2Double::const_iterator fi = cpd.begin(); fi != cpd.end(); ++fi) {
      if (fi->second > threshold || (vit.count(fi->first) > 0)) {
        cout << esym << ' ' << TD::Convert(fi->first) << ' ' << log(fi->second) << endl;
      }
    } 
  }
  return 0;
}

