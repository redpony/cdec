#ifndef RSBOTTOM_UP_PARSER_H_
#define RSBOTTOM_UP_PARSER_H_

#include <vector>
#include <string>

#include "lattice.h"
#include "grammar.h"

class Hypergraph;

// implementation of Sennrich (2014) parser
// http://aclweb.org/anthology/W/W14/W14-4011.pdf
class RSExhaustiveBottomUpParser {
 public:
  RSExhaustiveBottomUpParser(const std::string& goal_sym,
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
