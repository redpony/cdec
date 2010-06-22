#ifndef _ALIGNER_H_

#include <string>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include "array2d.h"
#include "lattice.h"

class Hypergraph;
class SentenceMetadata;

struct AlignerTools {
  static boost::shared_ptr<Array2D<bool> > ReadPharaohAlignmentGrid(const std::string& al);
  static void SerializePharaohFormat(const Array2D<bool>& alignment, std::ostream* out);

  // assumption: g contains derivations of input/ref and
  // ONLY input/ref.
  // if edges is non-NULL, the alignment corresponding to the edge rules will be written
  static void WriteAlignment(const Lattice& src,
                             const Lattice& ref,
                             const Hypergraph& g,
                             std::ostream* out,
                             bool map_instead_of_viterbi = true,
                             const std::vector<bool>* edges = NULL);
};

#endif
