#ifndef _FOREST_WRITER_H_
#define _FOREST_WRITER_H_

#include <string>

class Hypergraph;

struct ForestWriter {
  ForestWriter(const std::string& path, int num);
  bool Write(const Hypergraph& forest, bool minimal_rules);

  const std::string fname_;
  bool used_;
};

#endif
