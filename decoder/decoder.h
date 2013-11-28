#ifndef _DECODER_H_
#define _DECODER_H_

#include <iostream>
#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/program_options/variables_map.hpp>

#include "weights.h"  // weight_t

#undef CP_TIME
//#define CP_TIME
#ifdef CP_TIME
#include <time.h>
struct CpTime{
public:
	static void Add(clock_t x);
	static void Sub(clock_t x);
	static double Get();
private:
    static clock_t time_;
};
#endif

class SentenceMetadata;
class Hypergraph;
class DecoderImpl;

class DecoderObserver {
 public:
  virtual ~DecoderObserver();
  virtual void NotifyDecodingStart(const SentenceMetadata& smeta);
  virtual void NotifySourceParseFailure(const SentenceMetadata& smeta);
  virtual void NotifyTranslationForest(const SentenceMetadata& smeta, Hypergraph* hg);
  virtual void NotifyAlignmentFailure(const SentenceMetadata& semta);
  virtual void NotifyAlignmentForest(const SentenceMetadata& smeta, Hypergraph* hg);
  virtual void NotifyDecodingComplete(const SentenceMetadata& smeta);
};

class Grammar;  // TODO once the decoder interface is cleaned up,
                // this should be somewhere else
class Decoder {
 public:
  Decoder(int argc, char** argv);
  Decoder(std::istream* config_file);
  bool Decode(const std::string& input, DecoderObserver* observer = NULL);

  // access this to either *read* or *write* to the decoder's last
  // weight vector (i.e., the weights of the finest past)
  std::vector<weight_t>& CurrentWeightVector();
  const std::vector<weight_t>& CurrentWeightVector() const;

  // this sets the current sentence ID
  void SetId(int id);
  ~Decoder();
  const boost::program_options::variables_map& GetConf() const { return conf; }

  // add grammar rules (currently only supported by SCFG decoders)
  // that will be used on subsequent calls to Decode. rules should be in standard
  // text format. This function does NOT read from a file.
  void AddSupplementalGrammar(boost::shared_ptr<Grammar> gp);
  void AddSupplementalGrammarFromString(const std::string& grammar_string);
 private:
  boost::program_options::variables_map conf;
  boost::shared_ptr<DecoderImpl> pimpl_;
};

#endif
