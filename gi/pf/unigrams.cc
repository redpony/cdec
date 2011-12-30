#include "unigrams.h"

#include <string>
#include <cmath>

#include "stringlib.h"
#include "filelib.h"

using namespace std;

void UnigramModel::LoadUnigrams(const string& fname) {
  cerr << "Loading unigram probabilities from " << fname << " ..." << endl;
  ReadFile rf(fname);
  string line;
  istream& in = *rf.stream();
  assert(in);
  getline(in, line);
  assert(line.empty());
  getline(in, line);
  assert(line == "\\data\\");
  getline(in, line);
  size_t pos = line.find("ngram 1=");
  assert(pos == 0);
  assert(line.size() > 8);
  const size_t num_unigrams = atoi(&line[8]);
  getline(in, line);
  assert(line.empty());
  getline(in, line);
  assert(line == "\\1-grams:");
  for (size_t i = 0; i < num_unigrams; ++i) {
    getline(in, line);
    assert(line.size() > 0);
    pos = line.find('\t');
    assert(pos > 0);
    assert(pos + 1 < line.size());
    const WordID w = TD::Convert(line.substr(pos + 1));
    line[pos] = 0;
    float p = atof(&line[0]);
    if (w < probs_.size()) probs_[w].logeq(p * log(10)); else cerr << "WARNING: don't know about '" << TD::Convert(w) << "'\n";
  }
}

void UnigramWordModel::LoadUnigrams(const string& fname) {
  cerr << "Loading unigram probabilities from " << fname << " ..." << endl;
  ReadFile rf(fname);
  string line;
  istream& in = *rf.stream();
  assert(in);
  getline(in, line);
  assert(line.empty());
  getline(in, line);
  assert(line == "\\data\\");
  getline(in, line);
  size_t pos = line.find("ngram 1=");
  assert(pos == 0);
  assert(line.size() > 8);
  const size_t num_unigrams = atoi(&line[8]);
  getline(in, line);
  assert(line.empty());
  getline(in, line);
  assert(line == "\\1-grams:");
  for (size_t i = 0; i < num_unigrams; ++i) {
    getline(in, line);
    assert(line.size() > 0);
    pos = line.find('\t');
    assert(pos > 0);
    assert(pos + 1 < line.size());
    size_t cur = pos + 1;
    vector<WordID> w;
    while (cur < line.size()) {
      const size_t len = UTF8Len(line[cur]);
      w.push_back(TD::Convert(line.substr(cur, len)));
      cur += len;
    }
    line[pos] = 0;
    float p = atof(&line[0]);
    probs_[w].logeq(p * log(10.0));
  }
}

