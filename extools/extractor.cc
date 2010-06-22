#include <iostream>
#include <vector>
#include <utility>
#include <tr1/unordered_map>

#include <boost/functional/hash.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/lexical_cast.hpp>

#include "sparse_vector.h"
#include "sentence_pair.h"
#include "extract.h"
#include "tdict.h"
#include "fdict.h"
#include "wordid.h"
#include "array2d.h"
#include "filelib.h"

using namespace std;
using namespace std::tr1;
namespace po = boost::program_options;

static const size_t MAX_LINE_LENGTH = 100000;
WordID kBOS, kEOS, kDIVIDER, kGAP;
int kCOUNT;

void InitCommandLine(int argc, char** argv, po::variables_map* conf) {
  po::options_description opts("Configuration options");
  opts.add_options()
        ("input,i", po::value<string>()->default_value("-"), "Input file")
        ("default_category,d", po::value<string>(), "Default span type (use X for 'Hiero')")
        ("loose", "Use loose phrase extraction heuristic for base phrases")
        ("base_phrase,B", "Write base phrases")
        ("base_phrase_spans", "Write base sentences and phrase spans")
        ("bidir,b", "Extract bidirectional rules (for computing p(f|e) in addition to p(e|f))")
        ("combiner_size,c", po::value<size_t>()->default_value(800000), "Number of unique items to store in cache before writing rule counts. Set to 0 to disable cache.")
        ("silent", "Write nothing to stderr except errors")
        ("phrase_context,C", "Write base phrase contexts")
        ("phrase_context_size,S", po::value<int>()->default_value(2), "Use this many words of context on left and write when writing base phrase contexts")
        ("max_base_phrase_size,L", po::value<int>()->default_value(10), "Maximum starting phrase size")
        ("max_syms,l", po::value<int>()->default_value(5), "Maximum number of symbols in final phrase size")
        ("max_vars,v", po::value<int>()->default_value(2), "Maximum number of nonterminal variables in final phrase size")
        ("permit_adjacent_nonterminals,A", "Permit adjacent nonterminals in source side of rules")
        ("no_required_aligned_terminal,n", "Do not require an aligned terminal")
        ("help,h", "Print this help message and exit");
  po::options_description clo("Command line options");
  po::options_description dcmdline_options;
  dcmdline_options.add(opts);

  po::store(parse_command_line(argc, argv, dcmdline_options), *conf);
  po::notify(*conf);

  if (conf->count("help") || conf->count("input") == 0) {
    cerr << "\nUsage: extractor [-options]\n";
    cerr << dcmdline_options << endl;
    exit(1);
  }
}

// TODO how to handle alignment information?
void WriteBasePhrases(const AnnotatedParallelSentence& sentence,
                      const vector<ParallelSpan>& phrases) {
  vector<WordID> e,f;
  for (int it = 0; it < phrases.size(); ++it) {
    const ParallelSpan& phrase = phrases[it];
    e.clear();
    f.clear();
    for (int i = phrase.i1; i < phrase.i2; ++i)
      f.push_back(sentence.f[i]);
    for (int j = phrase.j1; j < phrase.j2; ++j)
      e.push_back(sentence.e[j]);
    cout << TD::GetString(f) << " ||| " << TD::GetString(e) << endl;
  }
}

void WriteBasePhraseSpans(const AnnotatedParallelSentence& sentence,
                          const vector<ParallelSpan>& phrases) {
  cout << TD::GetString(sentence.f) << " ||| " << TD::GetString(sentence.e) << " |||";
  for (int it = 0; it < phrases.size(); ++it) {
    const ParallelSpan& phrase = phrases[it];
    cout << " " << phrase.i1 << "-" << phrase.i2 
      << "-" << phrase.j1 << "-" << phrase.j2;
  }
  cout << endl;
}

struct CountCombiner {
  CountCombiner(size_t csize) : combiner_size(csize) {}
  ~CountCombiner() {
    if (!cache.empty()) WriteAndClearCache();
  }

  void Count(const vector<WordID>& key,
             const vector<WordID>& val,
             const int count_type,
             const vector<pair<short,short> >& aligns) {
    if (combiner_size > 0) {
      RuleStatistics& v = cache[key][val];
      float newcount = v.counts.add_value(count_type, 1.0f);
      // hack for adding alignments
      if (newcount < 7.0f && aligns.size() > v.aligns.size())
        v.aligns = aligns;
      if (cache.size() > combiner_size) WriteAndClearCache();
    } else {
      cout << TD::GetString(key) << '\t' << TD::GetString(val) << " ||| ";
      cout << RuleStatistics(count_type, 1.0f, aligns) << endl;
    }
  }

 private:
  void WriteAndClearCache() {
    for (unordered_map<vector<WordID>, Vec2PhraseCount, boost::hash<vector<WordID> > >::iterator it = cache.begin();
         it != cache.end(); ++it) {
      cout << TD::GetString(it->first) << '\t';
      const Vec2PhraseCount& vals = it->second;
      bool needdiv = false;
      for (Vec2PhraseCount::const_iterator vi = vals.begin(); vi != vals.end(); ++vi) {
        if (needdiv) cout << " ||| "; else needdiv = true;
        cout << TD::GetString(vi->first) << " ||| " << vi->second;
      }
      cout << endl;
    }
    cache.clear();
  }

  const size_t combiner_size;
  typedef unordered_map<vector<WordID>, RuleStatistics, boost::hash<vector<WordID> > > Vec2PhraseCount;
  unordered_map<vector<WordID>, Vec2PhraseCount, boost::hash<vector<WordID> > > cache;
};

// TODO optional source context
// output <k, v> : k = phrase "document" v = context "term"
void WritePhraseContexts(const AnnotatedParallelSentence& sentence,
                         const vector<ParallelSpan>& phrases,
                         const int ctx_size,
                         CountCombiner* o) {
  vector<WordID> context(ctx_size * 2 + 1);
  context[ctx_size] = kGAP;
  vector<WordID> key;
  key.reserve(100);
  for (int it = 0; it < phrases.size(); ++it) {
    const ParallelSpan& phrase = phrases[it];

    // TODO, support src keys as well
    key.resize(phrase.j2 - phrase.j1);
    for (int j = phrase.j1; j < phrase.j2; ++j)
      key[j - phrase.j1] = sentence.e[j];

    for (int i = 0; i < ctx_size; ++i) {
      int epos = phrase.j1 - 1 - i;
      const WordID left_ctx = (epos < 0) ? kBOS : sentence.e[epos];
      context[ctx_size - i - 1] = left_ctx;
      epos = phrase.j2 + i;
      const WordID right_ctx = (epos >= sentence.e_len) ? kEOS : sentence.e[epos];
      context[ctx_size + i + 1] = right_ctx;
    }
    o->Count(key, context, kCOUNT, vector<pair<short,short> >());
  }
}

struct SimpleRuleWriter : public Extract::RuleObserver {
 protected:
  virtual void CountRuleImpl(WordID lhs,
                             const vector<WordID>& rhs_f,
                             const vector<WordID>& rhs_e,
                             const vector<pair<short,short> >& fe_terminal_alignments) {
    cout << "[" << TD::Convert(-lhs) << "] |||";
    for (int i = 0; i < rhs_f.size(); ++i) {
      if (rhs_f[i] < 0) cout << " [" << TD::Convert(-rhs_f[i]) << ']';
      else cout << ' ' << TD::Convert(rhs_f[i]);
    }
    cout << " |||";
    for (int i = 0; i < rhs_e.size(); ++i) {
      if (rhs_e[i] <= 0) cout << " [" << (1-rhs_e[i]) << ']';
      else cout << ' ' << TD::Convert(rhs_e[i]);
    }
    cout << " |||";
    for (int i = 0; i < fe_terminal_alignments.size(); ++i) {
      cout << ' ' << fe_terminal_alignments[i].first << '-' << fe_terminal_alignments[i].second;
    }
    cout << endl;
  }
};

struct HadoopStreamingRuleObserver : public Extract::RuleObserver {
  HadoopStreamingRuleObserver(CountCombiner* cc, bool bidir_flag) :
     bidir(bidir_flag),
     kF(TD::Convert("F")),
     kE(TD::Convert("E")),
     kDIVIDER(TD::Convert("|||")),
     kLB("["), kRB("]"),
     combiner(*cc),
     kEMPTY(),
     kCFE(FD::Convert("CFE")) {
   for (int i=1; i < 50; ++i)
     index2sym[1-i] = TD::Convert(kLB + boost::lexical_cast<string>(i) + kRB);
   fmajor_key.resize(10, kF);
   emajor_key.resize(10, kE);
   if (bidir)
     fmajor_key[2] = emajor_key[2] = kDIVIDER;
   else
     fmajor_key[1] = kDIVIDER;
 }

 protected:
  virtual void CountRuleImpl(WordID lhs,
                             const vector<WordID>& rhs_f,
                             const vector<WordID>& rhs_e,
                             const vector<pair<short,short> >& fe_terminal_alignments) {
    if (bidir) { // extract rules in "both directions" E->F and F->E
      fmajor_key.resize(3 + rhs_f.size());
      emajor_key.resize(3 + rhs_e.size());
      fmajor_val.resize(rhs_e.size());
      emajor_val.resize(rhs_f.size());
      emajor_key[1] = fmajor_key[1] = MapSym(lhs);
      int nt = 1;
      for (int i = 0; i < rhs_f.size(); ++i) {
        const WordID id = rhs_f[i];
        if (id < 0) {
          fmajor_key[3 + i] = MapSym(id, nt);
          emajor_val[i] = MapSym(id, nt);
          ++nt;
        } else {
          fmajor_key[3 + i] = id;
          emajor_val[i] = id;
        }
      }
      for (int i = 0; i < rhs_e.size(); ++i) {
        WordID id = rhs_e[i];
        if (id <= 0) {
          fmajor_val[i] = index2sym[id];
          emajor_key[3 + i] = index2sym[id];
        } else {
          fmajor_val[i] = id;
          emajor_key[3 + i] = id;
        }
      }
      combiner.Count(fmajor_key, fmajor_val, kCFE, fe_terminal_alignments);
      combiner.Count(emajor_key, emajor_val, kCFE, kEMPTY);
    } else { // extract rules only in F->E
      fmajor_key.resize(2 + rhs_f.size());
      fmajor_val.resize(rhs_e.size());
      fmajor_key[0] = MapSym(lhs);
      int nt = 1;
      for (int i = 0; i < rhs_f.size(); ++i) {
        const WordID id = rhs_f[i];
        if (id < 0)
          fmajor_key[2 + i] = MapSym(id, nt++);
        else
          fmajor_key[2 + i] = id;
      }
      for (int i = 0; i < rhs_e.size(); ++i) {
        const WordID id = rhs_e[i];
        if (id <= 0)
          fmajor_val[i] = index2sym[id];
        else
          fmajor_val[i] = id;
      }
      combiner.Count(fmajor_key, fmajor_val, kCFE, fe_terminal_alignments);
    }
  }

 private:
  WordID MapSym(WordID sym, int ind = 0) {
    WordID& r = cat2ind2sym[sym][ind];
    if (!r) {
      if (ind == 0)
        r = TD::Convert(kLB + TD::Convert(-sym) + kRB);
      else
        r = TD::Convert(kLB + TD::Convert(-sym) + "," + boost::lexical_cast<string>(ind) + kRB);
    }
    return r;
  }

  const bool bidir;
  const WordID kF, kE, kDIVIDER;
  const string kLB, kRB;
  CountCombiner& combiner;
  const vector<pair<short,short> > kEMPTY;
  const int kCFE;
  map<WordID, map<int, WordID> > cat2ind2sym;
  map<int, WordID> index2sym;
  vector<WordID> emajor_key, emajor_val, fmajor_key, fmajor_val;
};

int main(int argc, char** argv) {
  po::variables_map conf;
  InitCommandLine(argc, argv, &conf);
  kBOS = TD::Convert("<s>");
  kEOS = TD::Convert("</s>");
  kDIVIDER = TD::Convert("|||");
  kGAP = TD::Convert("<PHRASE>");
  kCOUNT = FD::Convert("C");

  WordID default_cat = 0;  // 0 means no default- extraction will
                           // fail if a phrase is extracted without a
                           // category
  if (conf.count("default_category")) {
    string sdefault_cat = conf["default_category"].as<string>();
    default_cat = -TD::Convert(sdefault_cat);
    cerr << "Default category: " << sdefault_cat << endl;
  } else {
    cerr << "No default category (use --default_category if you want to set one)\n";
  }
  ReadFile rf(conf["input"].as<string>());
  istream& in = *rf.stream();

  char buf[MAX_LINE_LENGTH];
  AnnotatedParallelSentence sentence;
  vector<ParallelSpan> phrases;
  const int max_base_phrase_size = conf["max_base_phrase_size"].as<int>();
  const bool write_phrase_contexts = conf.count("phrase_context") > 0;
  const bool write_base_phrases = conf.count("base_phrase") > 0;
  const bool write_base_phrase_spans = conf.count("base_phrase_spans") > 0;
  const bool loose_phrases = conf.count("loose") > 0;
  const bool silent = conf.count("silent") > 0;
  const int max_syms = conf["max_syms"].as<int>();
  const int max_vars = conf["max_vars"].as<int>();
  const int ctx_size = conf["phrase_context_size"].as<int>();
  const bool permit_adjacent_nonterminals = conf.count("permit_adjacent_nonterminals") > 0;
  const bool require_aligned_terminal = conf.count("no_required_aligned_terminal") == 0;
  int line = 0;
  CountCombiner cc(conf["combiner_size"].as<size_t>());
  HadoopStreamingRuleObserver o(&cc,
                                conf.count("bidir") > 0);
  //SimpleRuleWriter o;
  while(in) {
    ++line;
    in.getline(buf, MAX_LINE_LENGTH);
    if (buf[0] == 0) continue;
    if (!silent) {
      if (line % 200 == 0) cerr << '.';
      if (line % 8000 == 0) cerr << " [" << line << "]\n" << flush;
    }
    sentence.ParseInputLine(buf);
    phrases.clear();
    Extract::ExtractBasePhrases(max_base_phrase_size, sentence, &phrases);
    if (loose_phrases)
      Extract::LoosenPhraseBounds(sentence, max_base_phrase_size, &phrases);
    if (phrases.empty()) {
      cerr << "WARNING no phrases extracted line: " << line << endl;
      continue;
    }
    if (write_phrase_contexts) {
      WritePhraseContexts(sentence, phrases, ctx_size, &cc);
      continue;
    }
    if (write_base_phrases) {
      WriteBasePhrases(sentence, phrases);
      continue;
    }
    if (write_base_phrase_spans) {
      WriteBasePhraseSpans(sentence, phrases);
      continue;
    }
    Extract::AnnotatePhrasesWithCategoryTypes(default_cat, sentence.span_types, &phrases);
    Extract::ExtractConsistentRules(sentence, phrases, max_vars, max_syms, permit_adjacent_nonterminals, require_aligned_terminal, &o);
  }
  if (!silent) cerr << endl;
  return 0;
}

