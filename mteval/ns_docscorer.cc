#include "ns_docscorer.h"

#include <iostream>
#include <cstring>

#include "tdict.h"
#include "filelib.h"
#include "ns.h"

using namespace std;

DocumentScorer::~DocumentScorer() {}

DocumentScorer::DocumentScorer() {}

void DocumentScorer::Init(const EvaluationMetric* metric,
            const vector<string>& ref_files,
            const string& src_file,
            bool verbose) {
  scorers_.clear();
  static const WordID kDIV = TD::Convert("|||");
  if (verbose) cerr << "Loading references (" << ref_files.size() << " files)\n";
  assert(src_file.empty());
  std::vector<ReadFile> ifs(ref_files.begin(),ref_files.end());
  for (int i=0; i < ref_files.size(); ++i) ifs[i].Init(ref_files[i]);
  char buf[64000];
  bool expect_eof = false;
  int line=0;
  vector<WordID> tmp;
  vector<vector<WordID> > refs;
  while (ifs[0].get()) {
    refs.clear();
    for (int i=0; i < ref_files.size(); ++i) {
      istream &in=ifs[i].get();
      if (in.eof()) break;
      in.getline(buf, 64000);
      if (strlen(buf) == 0) {
        if (in.eof()) {
          if (!expect_eof) {
            assert(i == 0);
            expect_eof = true;
          }
          break;
        }
      } else { // read a line from a reference file
        tmp.clear();
        TD::ConvertSentence(buf, &tmp);
        unsigned last = 0;
        for (unsigned j = 0; j < tmp.size(); ++j) {
          if (tmp[j] == kDIV) {
            refs.push_back(vector<WordID>(tmp.begin() + last, tmp.begin() + j));
            last = j + 1;
          }
        }
        refs.push_back(vector<WordID>(tmp.begin() + last, tmp.end()));
      }
      assert(!expect_eof);
    }
    if (!expect_eof) {
      string src_line;
      //if (srcrf) {
      //  getline(srcrf.get(), src_line);
      //  map<string,string> dummy;
      //  ProcessAndStripSGML(&src_line, &dummy);
      //}
      scorers_.push_back(metric->CreateSegmentEvaluator(refs));
      ++line;
    }
  }
  if (verbose) cerr << "Loaded reference translations for " << scorers_.size() << " sentences.\n";
}

