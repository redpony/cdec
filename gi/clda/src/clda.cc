#include <iostream>
#include <vector>
#include <map>
#include <string>

#include "timer.h"
#include "crp.h"
#include "ccrp.h"
#include "sampler.h"
#include "tdict.h"
const size_t MAX_DOC_LEN_CHARS = 10000000;

using namespace std;

void ShowTopWordsForTopic(const map<WordID, int>& counts) {
  multimap<int, WordID> ms;
  for (map<WordID,int>::const_iterator it = counts.begin(); it != counts.end(); ++it)
    ms.insert(make_pair(it->second, it->first));
  int cc = 0;
  for (multimap<int, WordID>::reverse_iterator it = ms.rbegin(); it != ms.rend(); ++it) {
    cerr << it->first << ':' << TD::Convert(it->second) << " ";
    ++cc;
    if (cc==20) break;
  }
  cerr << endl;
}

void tc() {
  MT19937 rng;
  CCRP<string> crp(0.1, 5);
  double un = 0.25;
  int tt = 0;
  tt += crp.increment("hi", un, &rng);
  tt += crp.increment("foo", un, &rng);
  tt += crp.increment("bar", un, &rng);
  tt += crp.increment("bar", un, &rng);
  tt += crp.increment("bar", un, &rng);
  tt += crp.increment("bar", un, &rng);
  tt += crp.increment("bar", un, &rng);
  tt += crp.increment("bar", un, &rng);
  tt += crp.increment("bar", un, &rng);
  cout << "tt=" << tt << endl;
  cout << crp << endl;
  cout << "  P(bar)=" << crp.prob("bar", un) << endl;
  cout << "  P(hi)=" << crp.prob("hi", un) << endl;
  cout << "  P(baz)=" << crp.prob("baz", un) << endl;
  cout << "  P(foo)=" << crp.prob("foo", un) << endl;
  double x = crp.prob("bar", un) + crp.prob("hi", un) + crp.prob("baz", un) + crp.prob("foo", un);
  cout << "    tot=" << x << endl;
  tt += crp.decrement("hi", &rng);
  tt += crp.decrement("bar", &rng);
  cout << crp << endl;
  tt += crp.decrement("bar", &rng);
  cout << crp << endl;
  cout << "tt=" << tt << endl;
  cout << crp.log_crp_prob() << endl;
}

int main(int argc, char** argv) {
  tc();
  if (argc != 3) {
    cerr << "Usage: " << argv[0] << " num-classes num-samples\n";
    return 1;
  }
  const int num_classes = atoi(argv[1]);
  const int num_iterations = atoi(argv[2]);
  const int burnin_size = num_iterations * 0.9;
  if (num_classes < 2) {
    cerr << "Must request more than 1 class\n";
    return 1;
  }
  if (num_iterations < 5) {
    cerr << "Must request more than 5 iterations\n";
    return 1;
  }
  cerr << "CLASSES: " << num_classes << endl;
  char* buf = new char[MAX_DOC_LEN_CHARS];
  vector<vector<int> > wji;   // w[j][i] - observed word i of doc j
  vector<vector<int> > zji;   // z[j][i] - topic assignment for word i of doc j
  cerr << "READING DOCUMENTS\n";
  while(cin) {
    cin.getline(buf, MAX_DOC_LEN_CHARS);
    if (buf[0] == 0) continue;
    wji.push_back(vector<WordID>());
    TD::ConvertSentence(buf, &wji.back());
  }
  cerr << "READ " << wji.size() << " DOCUMENTS\n";
  MT19937 rng;
  cerr << "INITIALIZING RANDOM TOPIC ASSIGNMENTS\n";
  zji.resize(wji.size());
  double disc = 0.05;
  double beta = 10.0;
  double alpha = 50.0;
  double uniform_topic = 1.0 / num_classes;
  double uniform_word = 1.0 / TD::NumWords();
  vector<CCRP<int> > dr(zji.size(), CCRP<int>(disc, beta)); // dr[i] describes the probability of using a topic in document i
  vector<CCRP<int> > wr(num_classes, CCRP<int>(disc, alpha)); // wr[k] describes the probability of generating a word in topic k
  for (int j = 0; j < zji.size(); ++j) {
    const size_t num_words = wji[j].size();
    vector<int>& zj = zji[j];
    const vector<int>& wj = wji[j];
    zj.resize(num_words);
    for (int i = 0; i < num_words; ++i) {
      int random_topic = rng.next() * num_classes;
      if (random_topic == num_classes) { --random_topic; }
      zj[i] = random_topic;
      const int word = wj[i];
      dr[j].increment(random_topic, uniform_topic, &rng);
      wr[random_topic].increment(word, uniform_word, &rng);
    }
  }
  cerr << "SAMPLING\n";
  vector<map<WordID, int> > t2w(num_classes);
  Timer timer;
  SampleSet<double> ss;
  ss.resize(num_classes);
  double total_time = 0;
  for (int iter = 0; iter < num_iterations; ++iter) {
    cerr << '.';
    if (iter && iter % 10 == 0) {
      total_time += timer.Elapsed();
      timer.Reset();
      double llh = 0;
      for (int j = 0; j < dr.size(); ++j)
        llh += dr[j].log_crp_prob();
      for (int j = 0; j < wr.size(); ++j)
        llh += wr[j].log_crp_prob();
      cerr << " [LLH=" << llh << " I=" << iter << "]\n";
    }
    for (int j = 0; j < zji.size(); ++j) {
      const size_t num_words = wji[j].size();
      vector<int>& zj = zji[j];
      const vector<int>& wj = wji[j];
      for (int i = 0; i < num_words; ++i) {
        const int word = wj[i];
        const int cur_topic = zj[i];
        dr[j].decrement(cur_topic, &rng);
        wr[cur_topic].decrement(word, &rng);
 
        for (int k = 0; k < num_classes; ++k) {
          ss[k]= dr[j].prob(k, uniform_topic) * wr[k].prob(word, uniform_word);
        }
        const int new_topic = rng.SelectSample(ss);
        dr[j].increment(new_topic, uniform_topic, &rng);
        wr[new_topic].increment(word, uniform_word, &rng);
        zj[i] = new_topic;
        if (iter > burnin_size) {
          ++t2w[cur_topic][word];
        }
      }
    }
  }
  for (int i = 0; i < num_classes; ++i) {
    cerr << "---------------------------------\n";
    ShowTopWordsForTopic(t2w[i]);
  }
  cerr << "-------------\n";
#if 0
  for (int j = 0; j < zji.size(); ++j) {
    const size_t num_words = wji[j].size();
    vector<int>& zj = zji[j];
    const vector<int>& wj = wji[j];
    zj.resize(num_words);
    for (int i = 0; i < num_words; ++i) {
      cerr << TD::Convert(wji[j][i]) << '(' << zj[i] << ") ";
    }
    cerr << endl;
  }
#endif
  return 0;
}

