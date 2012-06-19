#ifndef _CORPUS_TOOLS_H_
#define _CORPUS_TOOLS_H_

#include <string>
#include <set>
#include <vector>
#include "wordid.h"

struct CorpusTools {
  static void ReadLine(const std::string& line,
                       std::vector<WordID>* src,
                       std::vector<WordID>* trg);

  static void ReadFromFile(const std::string& filename,
                           std::vector<std::vector<WordID> >* src,
                           std::set<WordID>* src_vocab = NULL,
                           std::vector<std::vector<WordID> >* trg = NULL,
                           std::set<WordID>* trg_vocab = NULL,
                           int rank = 0,
                           int size = 1);
};

#endif
