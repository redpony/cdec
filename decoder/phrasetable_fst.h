#ifndef _PHRASETABLE_FST_H_
#define _PHRASETABLE_FST_H_

#include <vector>
#include <string>

#include "sparse_vector.h"
#include "trule.h"

class TargetPhraseSet {
 public:
  virtual ~TargetPhraseSet();
  virtual const std::vector<TRulePtr>& GetRules() const = 0;
};

class FSTNode {
 public:
  virtual ~FSTNode();
  virtual const TargetPhraseSet* GetTranslations() const = 0;
  virtual bool HasData() const = 0;
  virtual bool HasOutgoingNonEpsilonEdges() const = 0;
  virtual const FSTNode* Extend(const WordID& t) const = 0;

  // these should only be called on q_0:
  virtual void AddPassThroughTranslation(const WordID& w, const SparseVector<double>& feats) = 0;
  virtual void ClearPassThroughTranslations() = 0;
};

// attn caller: you own the memory
FSTNode* LoadTextPhrasetable(const std::vector<std::string>& filenames);
FSTNode* LoadTextPhrasetable(std::istream* in);
FSTNode* LoadBinaryPhrasetable(const std::string& fname_prefix);

#endif
