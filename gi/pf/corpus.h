#ifndef _CORPUS_H_
#define _CORPUS_H_

#include <string>
#include <vector>
#include <set>
#include "wordid.h"

namespace corpus {

void ReadParallelCorpus(const std::string& filename,
                std::vector<std::vector<WordID> >* f,
                std::vector<std::vector<WordID> >* e,
                std::set<WordID>* vocab_f,
                std::set<WordID>* vocab_e);

}

#endif
