#include "forest_writer.h"

#include <iostream>

#include "fast_lexical_cast.hpp"

#include "filelib.h"
#include "hg_io.h"
#include "hg.h"

using namespace std;

ForestWriter::ForestWriter(const std::string& path, int num) :
  fname_(path + '/' + boost::lexical_cast<string>(num) + ".json.gz"), used_(false) {}

bool ForestWriter::Write(const Hypergraph& forest, bool minimal_rules) {
  assert(!used_);
  used_ = true;
  cerr << "  Writing forest to " << fname_ << endl;
  WriteFile wf(fname_);
  return HypergraphIO::WriteToJSON(forest, minimal_rules, wf.stream());
}

