#include <iostream>
#include <vector>
#include <map>

#include "timer.h"
#include "crp.h"
#include "sampler.h"
#include "tdict.h"
Dict TD::dict_;
std::string TD::empty = "";
std::string TD::space = " ";
const size_t MAX_DOC_LEN_CHARS = 1000000;

using namespace std;

void ShowTopWordsForTopic(const map<WordID, int>& counts) {
  multimap<int, WordID> ms;
  for (map<WordID,int>::const_iterator it = counts.begin(); it != counts.end(); ++it)
    ms.insert(make_pair(it->second, it->first));
  int cc = 0;
  for (multimap<int, WordID>::reverse_iterator it = ms.rbegin(); it != ms.rend(); ++it) {
    cerr << it->first << ':' << TD::Convert(it->second) << " ";
    ++cc;
    if (cc==12) break;
  }
  cerr << endl;
}

int main(int argc, char** argv) {
  if (argc != 3) {
    cerr << "Usage: " << argv[0] << " num-classes num-samples\n";
    return 1;
  }
  const int num_classes = atoi(argv[1]);
  const int num_iterations = atoi(argv[2]);
  const int burnin_size = num_iterations * 0.666;
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
  double beta = 0.01;
  double alpha = 0.001;
  vector<CRP<int> > dr(zji.size(), CRP<int>(beta)); // dr[i] describes the probability of using a topic in document i
  vector<CRP<int> > wr(num_classes, CRP<int>(alpha)); // wr[k] describes the probability of generating a word in topic k
      int random_topic = rng.next() * num_classes;
  for (int j = 0; j < zji.size(); ++j) {
    const size_t num_words = wji[j].size();
    vector<int>& zj = zji[j];
    const vector<int>& wj = wji[j];
    zj.resize(num_words);
    for (int i = 0; i < num_words; ++i) {
      if (random_topic == num_classes) { --random_topic; }
      zj[i] = random_topic;
      const int word = wj[i];
      dr[j].increment(random_topic);
      wr[random_topic].increment(word);
    }
  }
  cerr << "SAMPLING\n";
  vector<map<WordID, int> > t2w(num_classes);
  Timer timer;
  SampleSet ss;
  ss.resize(num_classes);
  double total_time = 0;
  for (int iter = 0; iter < num_iterations; ++iter) {
    cerr << '.';
    if (iter && iter % 10 == 0) {
      total_time += timer.Elapsed();
      timer.Reset();
      prob_t lh = prob_t::One();
      for (int j = 0; j < zji.size(); ++j) {
        const size_t num_words = wji[j].size();
        vector<int>& zj = zji[j];
        const vector<int>& wj = wji[j];
        for (int i = 0; i < num_words; ++i) {
          const int word = wj[i];
          const int cur_topic = zj[i];
          lh *= dr[j].prob(cur_topic);
          lh *= wr[cur_topic].prob(word);
          if (iter > burnin_size) {
            ++t2w[cur_topic][word];
          }
        }
      }
      if (iter && iter % 40 == 0) { cerr << " [ITER=" << iter << " SEC/SAMPLE=" << (total_time / 40) << " LLH=" << log(lh) << "]\n"; total_time=0; }
      //cerr << "ITERATION " << iter << " LOG LIKELIHOOD: " << log(lh) << endl;
    }
    for (int j = 0; j < zji.size(); ++j) {
      const size_t num_words = wji[j].size();
      vector<int>& zj = zji[j];
      const vector<int>& wj = wji[j];
      for (int i = 0; i < num_words; ++i) {
        const int word = wj[i];
        const int cur_topic = zj[i];
        dr[j].decrement(cur_topic);
        wr[cur_topic].decrement(word);
 
        for (int k = 0; k < num_classes; ++k) {
          ss[k]= dr[j].prob(k) * wr[k].prob(word);
        }
        const int new_topic = rng.SelectSample(ss);
        dr[j].increment(new_topic);
        wr[new_topic].increment(word);
        zj[i] = new_topic;
      }
    }
  }
  for (int i = 0; i < num_classes; ++i) {
    cerr << "---------------------------------\n";
    ShowTopWordsForTopic(t2w[i]);
  }
  return 0;
}

