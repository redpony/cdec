#include "extract.h"

#include <queue>
#include <vector>
#include <utility>
#include <tr1/unordered_map>
#include <set>

#include <boost/functional/hash.hpp>

#include "sentence_pair.h"
#include "tdict.h"
#include "wordid.h"
#include "array2d.h"

using namespace std;
using namespace tr1;

namespace {
  inline bool IsWhitespace(char c) { return c == ' ' || c == '\t'; }

  inline void SkipWhitespace(const char* buf, int* ptr) {
    while (buf[*ptr] && IsWhitespace(buf[*ptr])) { ++(*ptr); }
  }
}

Extract::RuleObserver::~RuleObserver() {
  cerr << "Rules extracted: " << count << endl;
}

void Extract::ExtractBasePhrases(const int max_base_phrase_size,
                        const AnnotatedParallelSentence& sentence,
                        vector<ParallelSpan>* phrases) {
  phrases->clear();

  vector<pair<int,int> > f_spans(sentence.f_len, pair<int,int>(sentence.e_len, 0));
  vector<pair<int,int> > e_spans(sentence.e_len, pair<int,int>(sentence.f_len, 0));
  // for each alignment point in e, precompute the minimal consistent phrases in f
  // for each alignment point in f, precompute the minimal consistent phrases in e
  for (int i = 0; i < sentence.f_len; ++i) {
    for (int j = 0; j < sentence.e_len; ++j) {
      if (sentence.aligned(i,j)) {
        if (j < f_spans[i].first) f_spans[i].first = j;
        f_spans[i].second = j+1;
        if (i < e_spans[j].first) e_spans[j].first = i;
        e_spans[j].second = i+1;
      }
    }
  }

  for (int i1 = 0; i1 < sentence.f_len; ++i1) {
    if (sentence.f_aligned[i1] == 0) continue;
    int j1 = sentence.e_len;
    int j2 = 0;
    const int i_limit = min(sentence.f_len, i1 + max_base_phrase_size);
    for (int i2 = i1 + 1; i2 <= i_limit; ++i2) {
      if (sentence.f_aligned[i2-1] == 0) continue;
      // cerr << "F has aligned span " << i1 << " to " << i2 << endl;
      j1 = min(j1, f_spans[i2-1].first);
      j2 = max(j2, f_spans[i2-1].second);
      if (j1 >= j2) continue;
      if (j2 - j1 > max_base_phrase_size) continue;
      int condition = 0;
      for (int j = j1; j < j2; ++j) {
        if (e_spans[j].first < i1) { condition = 1; break; }
        if (e_spans[j].second > i2) { condition = 2; break; }
      }
      if (condition == 1) break;
      if (condition == 2) continue;
      // category types added later!
      phrases->push_back(ParallelSpan(i1, i2, j1, j2));
      // cerr << i1 << " " << i2 << " : " << j1 << " " << j2 << endl;
    }
  }
}

void Extract::LoosenPhraseBounds(const AnnotatedParallelSentence& sentence,
                                 const int max_base_phrase_size,
                                 vector<ParallelSpan>* phrases) {
  const int num_phrases = phrases->size();
  map<int, map<int, map<int, map<int, bool> > > > marker;
  for (int i = 0; i < num_phrases; ++i) {
    const ParallelSpan& cur = (*phrases)[i];
    marker[cur.i1][cur.i2][cur.j1][cur.j2] = true;
  }
  for (int i = 0; i < num_phrases; ++i) {
    const ParallelSpan& cur = (*phrases)[i];
    const int i1_max = cur.i1;
    const int i2_min = cur.i2;
    const int j1_max = cur.j1;
    const int j2_min = cur.j2;
    int i1_min = i1_max;
    while (i1_min > 0 && sentence.f_aligned[i1_min-1] == 0) { --i1_min; }
    int j1_min = j1_max;
    while (j1_min > 0 && sentence.e_aligned[j1_min-1] == 0) { --j1_min; }
    int i2_max = i2_min;
    while (i2_max < sentence.f_len && sentence.f_aligned[i2_max] == 0) { ++i2_max; }
    int j2_max = j2_min;
    while (j2_max < sentence.e_len && sentence.e_aligned[j2_max] == 0) { ++j2_max; }
    for (int i1 = i1_min; i1 <= i1_max; ++i1) {
      const int ilim = min(i2_max, i1 + max_base_phrase_size);
      for (int i2 = max(i1+1,i2_min); i2 <= ilim; ++i2) {
        for (int j1 = j1_min; j1 <= j1_max; ++j1) {
          const int jlim = min(j2_max, j1 + max_base_phrase_size);
          for (int j2 = max(j1+1, j2_min); j2 <= jlim; ++j2) {
            bool& seen = marker[i1][i2][j1][j2];
            if (!seen)
              phrases->push_back(ParallelSpan(i1,i2,j1,j2));
            seen = true;
          }
        }
      }
    }
  }
}

// this uses the TARGET span (i,j) to annotate phrases, will copy
// phrases if there is more than one annotation.
// TODO: support source annotation
void Extract::AnnotatePhrasesWithCategoryTypes(const WordID default_cat,
                                      const Array2D<vector<WordID> >& types,
                                      vector<ParallelSpan>* phrases) {
  const int num_unannotated_phrases = phrases->size();
  // have to use num_unannotated_phrases since we may grow the vector
  for (int i = 0; i < num_unannotated_phrases; ++i) {
    ParallelSpan& phrase = (*phrases)[i];
    const vector<WordID>* pcats = &types(phrase.j1, phrase.j2);
    if (pcats->empty() && default_cat != 0) {
      static vector<WordID> s_default(1, default_cat);
      pcats = &s_default;
    }
    if (pcats->empty()) {
      cerr << "ERROR span " << phrase.i1 << "," << phrase.i2 << "-"
           << phrase.j1 << "," << phrase.j2 << " has no type. "
              "Did you forget --default_category?\n";
    }
    const vector<WordID>& cats = *pcats;
    phrase.cat = cats[0];
    for (int ci = 1; ci < cats.size(); ++ci) {
      ParallelSpan new_phrase = phrase;
      new_phrase.cat = cats[ci];
      phrases->push_back(new_phrase);
    }
  }
}

// a partially complete (f-side) of a rule
struct RuleItem {
  vector<ParallelSpan> f;
  int i,j,syms,vars;
  explicit RuleItem(int pi) : i(pi), j(pi), syms(), vars() {}
  void Extend(const WordID& fword) {
    f.push_back(ParallelSpan(fword));
    ++j;
    ++syms;
  }
  void Extend(const ParallelSpan& subphrase) {
    f.push_back(subphrase);
    j += subphrase.i2 - subphrase.i1;
    ++vars;
    ++syms;
  }
  bool RuleFEndsInVariable() const {
    if (f.size() > 0) {
      return f.back().IsVariable();
    } else { return false; }
  }
};

void Extract::ExtractConsistentRules(const AnnotatedParallelSentence& sentence,
                          const vector<ParallelSpan>& phrases,
                          const int max_vars,
                          const int max_syms,
                          const bool permit_adjacent_nonterminals,
                          const bool require_aligned_terminal,
                          RuleObserver* observer) {
  queue<RuleItem> q;  // agenda for BFS
  int max_len = -1;
  unordered_map<pair<short, short>, vector<ParallelSpan>, boost::hash<pair<short, short> > > fspans;
  vector<vector<ParallelSpan> > spans_by_start(sentence.f_len);
  set<int> starts;
  for (int i = 0; i < phrases.size(); ++i) {
    fspans[make_pair(phrases[i].i1,phrases[i].i2)].push_back(phrases[i]);
    max_len = max(max_len, phrases[i].i2 - phrases[i].i1);
    // have we already added a rule item starting at phrases[i].i1?
    if (starts.insert(phrases[i].i1).second)
      q.push(RuleItem(phrases[i].i1));
    spans_by_start[phrases[i].i1].push_back(phrases[i]);
  }
  starts.clear();
  vector<pair<int,int> > next_e(sentence.e_len);
  vector<WordID> cur_rhs_f, cur_rhs_e;
  vector<pair<short, short> > cur_terminal_align;
  vector<int> cur_es, cur_fs;
  while(!q.empty()) {
    const RuleItem& rule = q.front();

    // extend the partial rule
    if (rule.j < sentence.f_len && (rule.j - rule.i) < max_len && rule.syms < max_syms) {
      RuleItem ew = rule;

      // extend with a word
      ew.Extend(sentence.f[ew.j]);
      q.push(ew);

      // with variables
      if (rule.vars < max_vars &&
          !spans_by_start[rule.j].empty() &&
          ((!rule.RuleFEndsInVariable()) || permit_adjacent_nonterminals)) {
        const vector<ParallelSpan>& sub_phrases = spans_by_start[rule.j];
        for (int it = 0; it < sub_phrases.size(); ++it) {
          if (sub_phrases[it].i2 - sub_phrases[it].i1 + rule.j - rule.i <= max_len) {
            RuleItem ev = rule;
            ev.Extend(sub_phrases[it]);
            q.push(ev);
            assert(ev.j <= sentence.f_len);
          }
        }
      }
    }
    // determine if rule is consistent
    if (rule.syms > 0 &&
        fspans.count(make_pair(rule.i,rule.j)) &&
        (!rule.RuleFEndsInVariable() || rule.syms > 1)) {
      const vector<ParallelSpan>& orig_spans = fspans[make_pair(rule.i,rule.j)];
      for (int s = 0; s < orig_spans.size(); ++s) {
        const ParallelSpan& orig_span = orig_spans[s];
        const WordID lhs = orig_span.cat;
        for (int j = orig_span.j1; j < orig_span.j2; ++j) next_e[j].first = -1;
        int nt_index_e = 0;
        for (int i = 0; i < rule.f.size(); ++i) {
          const ParallelSpan& cur = rule.f[i];
          if (cur.IsVariable())
            next_e[cur.j1] = pair<int,int>(cur.j2, ++nt_index_e);
        }
        cur_rhs_f.clear();
        cur_rhs_e.clear();
        cur_terminal_align.clear();
        cur_fs.clear();
        cur_es.clear();

        const int elen = orig_span.j2 - orig_span.j1;
        vector<int> isvar(elen, 0);
        int fbias = rule.i;
        bool bad_rule = false;
        bool has_aligned_terminal = false;
        for (int i = 0; i < rule.f.size(); ++i) {
          const ParallelSpan& cur = rule.f[i];
          cur_rhs_f.push_back(cur.cat);
          if (cur.cat > 0) {   // terminal
            if (sentence.f_aligned[fbias + i]) has_aligned_terminal = true;
            cur_fs.push_back(fbias + i);
          } else {             // non-terminal
            int subj1 = cur.j1 - orig_span.j1;
            int subj2 = cur.j2 - orig_span.j1;
            if (subj1 < 0 || subj2 > elen) { bad_rule = true; break; }
            for (int j = subj1; j < subj2 && !bad_rule; ++j) {
              int& isvarj = isvar[j];
              isvarj = true;
            }
            if (bad_rule) break;
            cur_fs.push_back(-1);
            fbias += cur.i2 - cur.i1 - 1;
          }
        }
        if (require_aligned_terminal && !has_aligned_terminal) bad_rule = true;
        if (!bad_rule) {
          for (int j = orig_span.j1; j < orig_span.j2; ++j) {
            if (next_e[j].first < 0) {
              cur_rhs_e.push_back(sentence.e[j]);
              cur_es.push_back(j);
            } else {
              cur_rhs_e.push_back(1 - next_e[j].second);  // next_e[j].second is NT gap index
              cur_es.push_back(-1);
              j = next_e[j].first - 1;
            }
          }
          for (short i = 0; i < cur_fs.size(); ++i)
            if (cur_fs[i] >= 0)
              for (short j = 0; j < cur_es.size(); ++j)
                if (cur_es[j] >= 0 && sentence.aligned(cur_fs[i],cur_es[j]))
                  cur_terminal_align.push_back(make_pair(i,j));
          observer->CountRule(lhs, cur_rhs_f, cur_rhs_e, cur_terminal_align);
        }
      }
    }
    q.pop();
  }
}

void RuleStatistics::ParseRuleStatistics(const char* buf, int start, int end) {
  int ptr = start;
  counts.clear();
  aligns.clear();
  while (ptr < end) {
    SkipWhitespace(buf, &ptr);
    int vstart = ptr;
    while(ptr < end && buf[ptr] != '=') ++ptr;
    assert(buf[ptr] == '=');
    assert(ptr > vstart);
    if (buf[vstart] == 'A' && buf[vstart+1] == '=') {
      ++ptr;
      while (ptr < end && !IsWhitespace(buf[ptr])) {
        while(ptr < end && buf[ptr] == ',') { ++ptr; }
        assert(ptr < end);
        vstart = ptr;
        while(ptr < end && buf[ptr] != ',' && !IsWhitespace(buf[ptr])) { ++ptr; }
        if (ptr > vstart) {
          short a, b;
          AnnotatedParallelSentence::ReadAlignmentPoint(buf, vstart, ptr, false, &a, &b);
          aligns.push_back(make_pair(a,b));
        }
      }
    } else {
      int name = FD::Convert(string(buf,vstart,ptr-vstart));
      ++ptr;
      vstart = ptr;
      while(ptr < end && !IsWhitespace(buf[ptr])) { ++ptr; }
      assert(ptr > vstart);
      counts.set_value(name, strtod(buf + vstart, NULL));
    }
  }
}

ostream& operator<<(ostream& os, const RuleStatistics& s) {
  bool needspace = false;
  for (SparseVector<float>::const_iterator it = s.counts.begin(); it != s.counts.end(); ++it) {
    if (needspace) os << ' '; else needspace = true;
    os << FD::Convert(it->first) << '=' << it->second;
  }
  if (s.aligns.size() > 0) {
    os << " A=";
    needspace = false;
    for (int i = 0; i < s.aligns.size(); ++i) {
      if (needspace) os << ','; else needspace = true;
      os << s.aligns[i].first << '-' << s.aligns[i].second;
    }
  }
  return os;
}

