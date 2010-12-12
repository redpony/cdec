#include "translator.h"

#include <iostream>
#include <vector>

#include "verbose.h"

using namespace std;

Translator::~Translator() {}

std::string Translator::GetDecoderType() const {
  return "UNKNOWN";
}

void Translator::ProcessMarkupHints(const map<string, string>& kv) {
  if (state_ != kUninitialized) {
    cerr << "Translator::ProcessMarkupHints in wrong state: " << state_ << endl;
    abort();
  }
  ProcessMarkupHintsImpl(kv);
  state_ = kReadyToTranslate;
}

bool Translator::Translate(const std::string& src,
                 SentenceMetadata* smeta,
                 const std::vector<double>& weights,
                 Hypergraph* minus_lm_forest) {
  if (state_ == kUninitialized) {
    cerr << "Translator::Translate(...) must not be in uninitialized state!\n";
    abort();
  }
  const bool result = TranslateImpl(src, smeta, weights, minus_lm_forest);
  state_ = kTranslated;
  return result;
}

void Translator::SentenceComplete() {
  if (state_ != kTranslated) {
    cerr << "Translator::Complete in unexpected state: " << state_ << endl;
    // not fatal
  }
  SentenceCompleteImpl();
  state_ = kUninitialized;  // return to start state
}

// this may be overridden by translators that want to accept
// metadata
void Translator::ProcessMarkupHintsImpl(const map<string, string>& kv) {
  int unprocessed = kv.size() - kv.count("id") - kv.count("psg");
  if (!SILENT) cerr << "Inside translator process hints\n";
  if (unprocessed > 0) {
    cerr << "Sentence markup contains unprocessed data:\n";
    for (map<string, string>::const_iterator it = kv.begin(); it != kv.end(); ++it) {
      if (it->first == "id") continue;
      cerr << "  KEY[" << it->first << "] --> " << it->second << endl;
    }
    abort();
  }
}

void Translator::SentenceCompleteImpl() {}

