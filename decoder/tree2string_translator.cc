#include <algorithm>
#include <vector>
#include <boost/functional/hash.hpp>
#include <unordered_map>
#include "tree_fragment.h"
#include "translator.h"
#include "hg.h"
#include "sentence_metadata.h"
#include "filelib.h"
#include "stringlib.h"
#include "tdict.h"
#include "verbose.h"

using namespace std;

// root: S
// A          implication: (S [A] *INCOMPLETE*
// B          implication: (S [A] [B] *INCOMPLETE*
// *0*        implication: (S _[A] [B])
// a          implication: (S (A a *INCOMPLETE* [B])
// a          implication: (S (A a a *INCOMPLETE* [B])
// *0*        implication: (S (A a a) _[B])
// D          implication: (S (A a a) (B [D] *INCOMPLETE*)
// *0*        implication: (S (A a a) (B _[D]))
// d          implication: (S (A a a) (B (D d *INCOMPLETE*))
// *0*        implication: (S (A a a) (B (D d)))
// --there are no further outgoing links possible--

// root: S
// A          implication: (S [A] *INCOMPLETE*
// B          implication: (S [A] [B] *INCOMPLETE*
// *0*        implication: (S _[A] [B])
// *0*        implication: (S [A] _[B])
// b          implication: (S [A] (B b *INCOMPLETE*))
struct Tree2StringGrammarNode {
  unordered_map<unsigned, Tree2StringGrammarNode> next;
  string rules;
};

void ReadTree2StringGrammar(istream* in, unordered_map<unsigned, Tree2StringGrammarNode>* proots) {
  unordered_map<unsigned, Tree2StringGrammarNode>& roots = *proots;
  string line;
  while(getline(*in, line)) {
    size_t pos = line.find("|||");
    assert(pos != string::npos);
    assert(pos > 3);
    if (line[pos - 1] == ' ') --pos;
    cdec::TreeFragment rule_src(line.substr(0, pos), true);
  }
}

struct Tree2StringTranslatorImpl {
  unordered_map<unsigned, Tree2StringGrammarNode> roots; // root['S'] gives rule network for S rules
  Tree2StringTranslatorImpl(const boost::program_options::variables_map& conf) {
    ReadFile rf(conf["grammar"].as<vector<string>>()[0]);
    ReadTree2StringGrammar(rf.stream(), &roots);
  }
  bool Translate(const string& input,
                 SentenceMetadata* smeta,
                 const vector<double>& weights,
                 Hypergraph* minus_lm_forest) {
    cdec::TreeFragment input_tree(input, false);
    cerr << "Tree2StringTranslatorImpl: please implement this!\n";
    return false;
  }
};

Tree2StringTranslator::Tree2StringTranslator(const boost::program_options::variables_map& conf) :
  pimpl_(new Tree2StringTranslatorImpl(conf)) {}

bool Tree2StringTranslator::TranslateImpl(const string& input,
                               SentenceMetadata* smeta,
                               const vector<double>& weights,
                               Hypergraph* minus_lm_forest) {
  return pimpl_->Translate(input, smeta, weights, minus_lm_forest);
}

void Tree2StringTranslator::ProcessMarkupHintsImpl(const map<string, string>& kv) {
}

void Tree2StringTranslator::SentenceCompleteImpl() {
}

std::string Tree2StringTranslator::GetDecoderType() const {
  return "tree2string";
}

