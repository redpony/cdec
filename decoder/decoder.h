#ifndef _DECODER_H_
#define _DECODER_H_

#include <iostream>
#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/program_options/variables_map.hpp>

class SentenceMetadata;
struct Hypergraph;
struct DecoderImpl;

struct DecoderObserver {
  virtual void NotifySourceParseFailure(const SentenceMetadata& smeta);
  virtual void NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg);
  virtual void NotifyAlignmentFailure(const SentenceMetadata& semta);
  virtual void NotifyAlignmentForest(const SentenceMetadata& smeta, Hypergraph* hg);
  virtual void NotifyDecodingComplete(const SentenceMetadata& smeta);
};

struct Decoder {
  Decoder(int argc, char** argv);
  Decoder(std::istream* config_file);
  bool Decode(const std::string& input, DecoderObserver* observer = NULL);
  void SetWeights(const std::vector<double>& weights);
  ~Decoder();
  const boost::program_options::variables_map& GetConf() const { return conf; }
 private:
  boost::program_options::variables_map conf;
  boost::shared_ptr<DecoderImpl> pimpl_;
};

#endif
