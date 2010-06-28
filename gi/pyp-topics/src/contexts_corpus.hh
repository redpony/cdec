#ifndef _CONTEXTS_CORPUS_HH
#define _CONTEXTS_CORPUS_HH

#include <vector>
#include <string>
#include <map>

#include <boost/ptr_container/ptr_vector.hpp>

#include "corpus.hh"
#include "contexts_lexer.h"
#include "../../../decoder/dict.h"

////////////////////////////////////////////////////////////////
// ContextsCorpus
////////////////////////////////////////////////////////////////

class ContextsCorpus : public Corpus {
  friend void read_callback(const ContextsLexer::PhraseContextsType&, void*);

public:
    typedef boost::ptr_vector<Document>::const_iterator const_iterator;

public:
    ContextsCorpus() {}
    virtual ~ContextsCorpus() {}

    unsigned read_contexts(const std::string &filename);

    TermBackoffPtr backoff_index() {
      return m_backoff;
    }

private:
    TermBackoffPtr m_backoff;
    Dict m_dict;
};

#endif // _CONTEXTS_CORPUS_HH
