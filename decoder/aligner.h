#ifndef _ALIGNER_H_

#include <string>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include "array2d.h"
#include "lattice.h"

class Hypergraph;
class SentenceMetadata;

struct AlignerTools {

  // assumption: g contains derivations of input/ref and
  // ONLY input/ref.
  // if edges is non-NULL, the alignment corresponding to the edge rules will be written
  static void WriteAlignment(const Lattice& src,
                             const Lattice& ref,
                             const Hypergraph& g,
                             std::ostream* out,
                             bool map_instead_of_viterbi = true,
                             int k_best = 0,
                             const std::vector<bool>* edges = NULL);
};

#endif
