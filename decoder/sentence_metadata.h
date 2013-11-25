#ifndef _SENTENCE_METADATA_H_
#define _SENTENCE_METADATA_H_

#include <string>
#include <map>
#include <cassert>
#include "lattice.h"

struct DocScorer;  // deprecated, will be removed
struct Score;     // deprecated, will be removed

class SentenceMetadata {
 public:
  friend class DecoderImpl;
  SentenceMetadata(int id, const Lattice& ref) :
    sent_id_(id),
    src_len_(-1),
    has_reference_(ref.size() > 0),
    trg_len_(ref.size()),
    ref_(has_reference_ ? &ref : NULL) {}

  int GetSentenceId() const { return sent_id_; }

  // this should be called by the Translator object after
  // it has parsed the source
  void SetSourceLength(int sl) { src_len_ = sl; }

  // this should be called if a separate model needs to
  // specify how long the target sentence should be
  void SetTargetLength(int tl) {
    assert(!has_reference_);
    trg_len_ = tl;
  }
  bool HasReference() const { return has_reference_; }
  const Lattice& GetReference() const { return *ref_; }
  int GetSourceLength() const { return src_len_; }
  int GetTargetLength() const { return trg_len_; }
  int GetSentenceID() const { return sent_id_; }
  // this will be empty if the translator accepts non FS input!
  const Lattice& GetSourceLattice() const { return src_lattice_; }

  // access to document level scores for MIRA vector computation
  void SetScore(Score *s){app_score=s;}
  void SetDocScorer (const DocScorer *d){ds = d;}
  void SetDocLen(double dl){doc_len = dl;}

  const Score& GetScore() const { return *app_score; }
  const DocScorer& GetDocScorer() const { return *ds; }
  double GetDocLen() const {return doc_len;}

  std::string GetSGMLValue(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = sgml_.find(key);
    if (it == sgml_.end()) return "";
    return it->second;
  }

 private:
  std::map<std::string, std::string> sgml_;
  const int sent_id_;
  // the following should be set, if possible, by the Translator
  int src_len_;
  double doc_len;
  const DocScorer* ds;
  const Score* app_score;
 public:
  Lattice src_lattice_;  // this will only be set if inputs are finite state!
 private:
  // you need to be very careful when depending on these values
  // they will only be set during training / alignment contexts
  const bool has_reference_;
  int trg_len_;
  const Lattice* const ref_;
};

#endif
