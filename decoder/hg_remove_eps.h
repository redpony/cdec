#ifndef _HG_REMOVE_EPS_H_
#define _HG_REMOVE_EPS_H_

#include "wordid.h"
class Hypergraph;

// This is not a complete implementation of the general algorithm for
// doing this. It makes a few weird assumptions, for example, that
// if some nonterminal X rewrites as eps, then that is the only thing
// that it rewrites as. This needs to be fixed for the general case!
void RemoveEpsilons(Hypergraph* g, WordID eps);

#endif
