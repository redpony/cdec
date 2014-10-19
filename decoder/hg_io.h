#ifndef HG_IO_H_
#define HG_IO_H_

#include <iostream>
#include <string>
#include "lattice.h"

class Hypergraph;

struct HypergraphIO {

  static bool ReadFromBinary(std::istream* in, Hypergraph* out);
  static bool WriteToBinary(const Hypergraph& hg, std::ostream* out);

  // if remove_rules is used, the hypergraph is serialized without rule information
  // (so it only contains structure and feature information)
  static void WriteAsCFG(const Hypergraph& hg);

  // Write only the target size information in bottom-up order.  
  static void WriteTarget(const std::string &base, unsigned int sent_id, const Hypergraph& hg);

  // serialization utils
  static void ReadFromPLF(const std::string& in, Hypergraph* out, int line = 0);
  // return PLF string representation (undefined behavior on non-lattices)
  static std::string AsPLF(const Hypergraph& hg, bool include_global_parentheses = true);
  static std::string AsPLF(const Lattice& lat, bool include_global_parentheses = true);
  static void PLFtoLattice(const std::string& plf, Lattice* pl);
  static std::string Escape(const std::string& s);  // PLF helper
};

#endif
