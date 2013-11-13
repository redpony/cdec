#ifndef _FACTORED_LEXICON_HELPER_
#define _FACTORED_LEXICON_HELPER_

#include <cassert>
#include <vector>
#include <string>
#include <map>
#include "tdict.h"

struct SentenceMetadata;

// when computing features, it can be advantageous to:
//   1) back off to less specific forms (e.g., less highly inflected forms, POS tags, etc)
//   2) look at more specific forms (on the source ONLY)
// this class helps you do both by creating a "corpus" view
// should probably add a discussion of why the source can be "refined" by this class
// but not the target. basically, this is because the source is on the right side of
// the conditioning line in the model, and the target is on the left. the most specific
// form must always be generated, but the "source" can include arbitrarily large
// context.
// this currently only works for sentence input to maintain simplicity of the code and
// file formats, but there is no reason why it couldn't work with lattices / CFGs
class FactoredLexiconHelper {
 public:
  // default constructor does no mapping
  FactoredLexiconHelper();
  // Either filename can be empty or * to indicate no mapping
  FactoredLexiconHelper(const std::string& srcfile, const std::string& trgmapfile);

  void PrepareForInput(const SentenceMetadata& smeta);

  inline WordID SourceWordAtPosition(const int i) const {
    if (i < 0) return kNULL;
    assert(i < cur_src_.size());
    return Escape(cur_src_[i]);
  }

  inline WordID CoarsenedTargetWordForTarget(const WordID surface_target) const {
    if (has_trg_) {
      const WordWordMap::const_iterator it = trgmap_.find(surface_target);
      if (it == trgmap_.end()) return surface_target;
      return Escape(it->second);
    } else {
      return Escape(surface_target);
    }
  }

 private:
  inline WordID Escape(WordID word) const {
    const std::map<WordID,WordID>::const_iterator it = escape_.find(word);
    if (it == escape_.end()) return word;
    return it->second;
  }

  void InitEscape();

  const WordID kNULL;
  bool has_src_;
  bool has_trg_;
  std::vector<std::vector<WordID> > src_;
  typedef std::map<WordID, WordID> WordWordMap;
  WordWordMap trgmap_;
  std::vector<WordID> cur_src_;
  std::map<WordID,WordID> escape_;
};

#endif
