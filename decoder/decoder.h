#ifndef _DECODER_H_
#define _DECODER_H_

#include <iostream>
#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

struct Hypergraph;
struct DecoderImpl;
struct Decoder {
  Decoder(int argc, char** argv);
  Decoder(std::istream* config_file);
  bool Decode(const std::string& input);
  bool DecodeProduceHypergraph(const std::string& input, Hypergraph* hg);
  void SetWeights(const std::vector<double>& weights);
  ~Decoder();
 private:
  boost::shared_ptr<DecoderImpl> pimpl_;
};

#endif
