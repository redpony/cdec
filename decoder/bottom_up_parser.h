#ifndef _BOTTOM_UP_PARSER_H_
#define _BOTTOM_UP_PARSER_H_

#include <vector>
#include <string>

#include "lattice.h"
#include "grammar.h"

class Hypergraph;

class ExhaustiveBottomUpParser {
 public:
  ExhaustiveBottomUpParser(const std::string& goal_sym,
                           const std::vector<GrammarPtr>& grammars);

  // returns true if goal reached spanning the full input
  // forest contains the full (i.e., unpruned) parse forest
  bool Parse(const Lattice& input,
             Hypergraph* forest) const;

 private:
  const std::string goal_sym_;
  const std::vector<GrammarPtr> grammars_;
};

#endif
