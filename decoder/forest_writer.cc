#include "forest_writer.h"

#include <iostream>

#include "fast_lexical_cast.hpp"

#include "filelib.h"
#include "hg_io.h"
#include "hg.h"

using namespace std;

ForestWriter::ForestWriter(const std::string& path, int num) :
  fname_(path + '/' + boost::lexical_cast<string>(num) + ".bin.gz"), used_(false) {}

bool ForestWriter::Write(const Hypergraph& forest) {
  assert(!used_);
  used_ = true;
  cerr << "  Writing forest to " << fname_ << endl;
  WriteFile wf(fname_);
  return HypergraphIO::WriteToBinary(forest, wf.stream());
}

