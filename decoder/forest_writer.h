#ifndef FOREST_WRITER_H_
#define FOREST_WRITER_H_

#include <string>

class Hypergraph;

struct ForestWriter {
  ForestWriter(const std::string& path, int num);
  bool Write(const Hypergraph& forest);

  const std::string fname_;
  bool used_;
};

#endif
