#ifndef PARSE_NL_H
#define	PARSE_NL_H

#include "smt_semparse_config.h"
#include "extract_request.h"
#include "decoder.h"
#include "../name_lexicon/nominatim_check.h"

#include <string>

using namespace std;

namespace smt_semparse {
  struct parseResult{
   string recover_query; // space separated without @
   string recover_fun; // functionalised version
   string mrl;
   string answer;
   string latlong;
  };

  struct preprocessed_sentence{
   string sentence;
   string stemmed;
   string non_stemmed;
  };

  /**
 * Data structure that given a smt-semparse model returns the answer for a new supplied sentence
 */
class NLParser {
 public:
  NLParser(string &exp_dir, string &db_dir, string &daemon_adress);
  NLParser(string &exp_dir, string &db_dir, SMTSemparseConfig &config_obj, string &daemon_adress);

  parseResult& parse_sentence(string &sentence, NominatimCheck* nom = NULL, string preset_tmp_dir = "", bool geojson = false, bool nom_lookup = false);
  preprocessed_sentence& preprocess_sentence(string &sentence);

 private:
  string sentence;
  string experiment_dir;
  string database_dir;
  SMTSemparseConfig config;
  extractor::Requester requester;
  struct preprocessed_sentence ps;
  struct parseResult parse_result;
};

} // namespace smt_semparse

#endif
