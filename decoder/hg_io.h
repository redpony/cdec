#ifndef _HG_IO_H_
#define _HG_IO_H_

#include <iostream>
#include <string>
#include "lattice.h"

class Hypergraph;

struct HypergraphIO {

  // the format is basically a list of nodes and edges in topological order
  // any edge you read, you must have already read its tail nodes
  // any node you read, you must have already read its incoming edges
  // this may make writing a bit more challenging if your forest is not
  // topologically sorted (but that probably doesn't happen very often),
  // but it makes reading much more memory efficient.
  // see test_data/small.json.gz for an email encoding
  static bool ReadFromJSON(std::istream* in, Hypergraph* out);

  // if remove_rules is used, the hypergraph is serialized without rule information
  // (so it only contains structure and feature information)
  static bool WriteToJSON(const Hypergraph& hg, bool remove_rules, std::ostream* out);

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
