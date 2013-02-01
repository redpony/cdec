#include "grammar_extractor.h"

#include <iterator>
#include <sstream>
#include <vector>

#include "grammar.h"
#include "rule.h"
#include "vocabulary.h"

using namespace std;

vector<string> Tokenize(const string& sentence) {
  vector<string> result;
  result.push_back("<s>");

  istringstream buffer(sentence);
  copy(istream_iterator<string>(buffer),
       istream_iterator<string>(),
       back_inserter(result));

  result.push_back("</s>");
  return result;
}

GrammarExtractor::GrammarExtractor(
    shared_ptr<SuffixArray> source_suffix_array,
    shared_ptr<DataArray> target_data_array,
    shared_ptr<Alignment> alignment, shared_ptr<Precomputation> precomputation,
    shared_ptr<Scorer> scorer, int min_gap_size, int max_rule_span,
    int max_nonterminals, int max_rule_symbols, int max_samples,
    bool use_baeza_yates, bool require_tight_phrases) :
    vocabulary(make_shared<Vocabulary>()),
    rule_factory(source_suffix_array, target_data_array, alignment,
        vocabulary, precomputation, scorer, min_gap_size, max_rule_span,
        max_nonterminals, max_rule_symbols, max_samples, use_baeza_yates,
        require_tight_phrases) {}

Grammar GrammarExtractor::GetGrammar(const string& sentence) {
  vector<string> words = Tokenize(sentence);
  vector<int> word_ids = AnnotateWords(words);
  return rule_factory.GetGrammar(word_ids);
}

vector<int> GrammarExtractor::AnnotateWords(const vector<string>& words) {
  vector<int> result;
  for (string word: words) {
    result.push_back(vocabulary->GetTerminalIndex(word));
  }
  return result;
}
