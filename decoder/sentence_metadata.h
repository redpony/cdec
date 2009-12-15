#ifndef _SENTENCE_METADATA_H_
#define _SENTENCE_METADATA_H_

#include <cassert>
#include "lattice.h"

struct SentenceMetadata {
  SentenceMetadata(int id, const Lattice& ref) :
    sent_id_(id),
    src_len_(-1),
    has_reference_(ref.size() > 0),
    trg_len_(ref.size()),
    ref_(has_reference_ ? &ref : NULL) {}

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

 private:
  const int sent_id_;
  // the following should be set, if possible, by the Translator
  int src_len_;
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
