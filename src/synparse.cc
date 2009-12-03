#include <iostream>
#include <ext/hash_map>
#include <ext/hash_set>
#include <utility>

#include <boost/multi_array.hpp>
#include <boost/functional/hash.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "prob.h"
#include "tdict.h"
#include "filelib.h"

using namespace std;
using namespace __gnu_cxx;
namespace po = boost::program_options;

const prob_t kMONO(1.0);  // 0.6
const prob_t kINV(1.0);   // 0.1
const prob_t kLEX(1.0);   // 0.3

typedef hash_map<vector<WordID>, hash_map<vector<WordID>, prob_t, boost::hash<vector<WordID> > >, boost::hash<vector<WordID> > > PTable;
typedef boost::multi_array<prob_t, 4> CChart;
typedef pair<int,int> SpanType;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("phrasetable,p",po::value<string>(), "[REQD] Phrase pairs for ITG alignment")
        ("input,i",po::value<string>()->default_value("-"), "Input file")
        ("help,h", "Help");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);
  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  bool flag = false;
  if (!conf->count("phrasetable")) {
    cerr << "Please specify a grammar file with -p <GRAMMAR.TXT>\n";
    flag = true;
  }
  if (flag || conf->count("help")) {
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

void LoadITGPhrasetable(const string& fname, PTable* ptable) {
  const WordID sep = TD::Convert("|||");
  ReadFile rf(fname);
  istream& in = *rf.stream();
  assert(in);
  int lc = 0;
  while(in) {
    string line;
    getline(in, line);
    if (line.empty()) continue;
    ++lc;
    vector<WordID> full, f, e;
    TD::ConvertSentence(line, &full);
    int i = 0;
    for (; i < full.size(); ++i) {
      if (full[i] == sep) break;
      f.push_back(full[i]);
    }
    ++i;
    for (; i < full.size(); ++i) {
      if (full[i] == sep) break;
      e.push_back(full[i]);
    }
    ++i;
    prob_t prob(0.000001);
    if (i < full.size()) { prob = prob_t(atof(TD::Convert(full[i]))); ++i; }

    if (i < full.size()) { cerr << "Warning line " << lc << " has extra stuff.\n"; }
    assert(f.size() > 0);
    assert(e.size() > 0);
    (*ptable)[f][e] = prob;
  }
  cerr << "Read " << lc << " phrase pairs\n";
}

void FindPhrases(const vector<WordID>& e, const vector<WordID>& f, const PTable& pt, CChart* pcc) {
  CChart& cc = *pcc;
  const size_t n = f.size();
  const size_t m = e.size();
  typedef hash_map<vector<WordID>, vector<SpanType>, boost::hash<vector<WordID> > > PhraseToSpan;
  PhraseToSpan e_locations;
  for (int i = 0; i < m; ++i) {
    const int mel = m - i;
    vector<WordID> e_phrase;
    for (int el = 0; el < mel; ++el) {
      e_phrase.push_back(e[i + el]);
      e_locations[e_phrase].push_back(make_pair(i, i + el + 1));
    }
  }
  //cerr << "Cached the locations of " << e_locations.size() << " e-phrases\n";

  for (int s = 0; s < n; ++s) {
    const int mfl = n - s;
    vector<WordID> f_phrase;
    for (int fl = 0; fl < mfl; ++fl) {
      f_phrase.push_back(f[s + fl]);
      PTable::const_iterator it = pt.find(f_phrase);
      if (it == pt.end()) continue;
      const hash_map<vector<WordID>, prob_t, boost::hash<vector<WordID> > >& es = it->second;
      for (hash_map<vector<WordID>, prob_t, boost::hash<vector<WordID> > >::const_iterator eit = es.begin(); eit != es.end(); ++eit) {
        PhraseToSpan::iterator loc = e_locations.find(eit->first);
        if (loc == e_locations.end()) continue;
        const vector<SpanType>& espans = loc->second;
        for (int j = 0; j < espans.size(); ++j) {
          cc[s][s + fl + 1][espans[j].first][espans[j].second] = eit->second;
          //cerr << '[' << s << ',' << (s + fl + 1) << ',' << espans[j].first << ',' << espans[j].second << "] is C\n";
        }
      }
    }
  }
}

long long int evals = 0;

void ProcessSynchronousCell(const int s,
                            const int t,
                            const int u,
                            const int v,
                            const prob_t& lex,
                            const prob_t& mono,
                            const prob_t& inv,
                            const CChart& tc, CChart* ntc) {
  prob_t& inside = (*ntc)[s][t][u][v];
  // cerr << log(tc[s][t][u][v]) << " + " << log(lex) << endl;
  inside = tc[s][t][u][v] * lex;
  // cerr << "  terminal span: " << log(inside) << endl;
  if (t - s == 1) return;
  if (v - u == 1) return;
  for (int x = s+1; x < t; ++x) {
    for (int y = u+1; y < v; ++y) {
      const prob_t m = (*ntc)[s][x][u][y] * (*ntc)[x][t][y][v] * mono;
      const prob_t i = (*ntc)[s][x][y][v] * (*ntc)[x][t][u][y] * inv;
      // cerr << log(i) << "\t" << log(m) << endl;
      inside += m;
      inside += i;
      evals++;
    }
  }
  // cerr << "           span: " << log(inside) << endl;
}

prob_t SynchronousParse(const int n, const int m, const prob_t& lex, const prob_t& mono, const prob_t& inv, const CChart& tc, CChart* ntc) {
  for (int fl = 0; fl < n; ++fl) {
    for (int el = 0; el < m; ++el) {
      const int ms = n - fl;
      for (int s = 0; s < ms; ++s) {
        const int t = s + fl + 1;
        const int mu = m - el;
        for (int u = 0; u < mu; ++u) {
          const int v = u + el + 1;
          //cerr << "Processing cell [" << s << ',' << t << ',' << u << ',' << v << "]\n";
          ProcessSynchronousCell(s, t, u, v, lex, mono, inv, tc, ntc);
        }
      }
    }
  }
  return (*ntc)[0][n][0][m];
}

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  PTable ptable;
  LoadITGPhrasetable(conf["phrasetable"].as<string>(), &ptable);
  ReadFile rf(conf["input"].as<string>());
  istream& in = *rf.stream();
  int lc = 0;
  const WordID sep = TD::Convert("|||");
  while(in) {
    string line;
    getline(in, line);
    if (line.empty()) continue;
    ++lc;
    vector<WordID> full, f, e;
    TD::ConvertSentence(line, &full);
    int i = 0;
    for (; i < full.size(); ++i) {
      if (full[i] == sep) break;
      f.push_back(full[i]);
    }
    ++i;
    for (; i < full.size(); ++i) {
      if (full[i] == sep) break;
      e.push_back(full[i]);
    }
    if (e.empty()) cerr << "E is empty!\n";
    if (f.empty()) cerr << "F is empty!\n";
    if (e.empty() || f.empty()) continue;
    int n = f.size();
    int m = e.size();
    cerr << "Synchronous chart has " << (n * n * m * m) << " cells\n";
    clock_t start = clock();
    CChart cc(boost::extents[n+1][n+1][m+1][m+1]);
    FindPhrases(e, f, ptable, &cc);
    CChart ntc(boost::extents[n+1][n+1][m+1][m+1]);
    prob_t likelihood = SynchronousParse(n, m, kLEX, kMONO, kINV, cc, &ntc);
    clock_t end = clock();
    cerr << "log Z: " << log(likelihood) << endl;
    cerr << "    Z: " << likelihood << endl;
    double etime = (end - start) / 1000000.0;
    cout << " time: " << etime << endl;
    cout << "evals: " << evals << endl;
  }
  return 0;
}

