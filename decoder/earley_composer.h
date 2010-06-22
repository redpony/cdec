#ifndef _EARLEY_COMPOSER_H_
#define _EARLEY_COMPOSER_H_

#include <iostream>

class EarleyComposerImpl;
class FSTNode;
class Hypergraph;

class EarleyComposer {
 public:
  ~EarleyComposer();
  EarleyComposer(const FSTNode* phrasetable_root);
  bool Compose(const Hypergraph& src_forest, Hypergraph* trg_forest);

  // reads the grammar from a file. There must be a single top-level
  // S -> X rule.  Anything else is possible. Format is:
  // [S] ||| [SS,1]
  // [SS] ||| [NP,1] [VP,2] ||| Feature1=0.2 Feature2=-2.3
  // [SS] ||| [VP,1] [NP,2] ||| Feature1=0.8
  // [NP] ||| [DET,1] [N,2] ||| Feature3=2
  // ...
  bool Compose(std::istream* grammar_file, Hypergraph* trg_forest);

 private:
  EarleyComposerImpl* pimpl_;
};

#endif
