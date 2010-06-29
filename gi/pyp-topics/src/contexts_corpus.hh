#ifndef _CONTEXTS_CORPUS_HH
#define _CONTEXTS_CORPUS_HH

#include <vector>
#include <string>
#include <map>

#include <boost/ptr_container/ptr_vector.hpp>

#include "corpus.hh"
#include "contexts_lexer.h"
#include "../../../decoder/dict.h"


class BackoffGenerator {
public:
  virtual ContextsLexer::Context
    operator()(const ContextsLexer::Context& c) = 0;

protected:
  ContextsLexer::Context strip_edges(const ContextsLexer::Context& c) {
    if (c.size() <= 1) return ContextsLexer::Context();
    assert(c.size() % 2 == 1);
    return ContextsLexer::Context(c.begin() + 1, c.end() - 1);
  }
};

class NullBackoffGenerator : public BackoffGenerator {
  virtual ContextsLexer::Context
    operator()(const ContextsLexer::Context&) 
    { return ContextsLexer::Context(); }
};

class SimpleBackoffGenerator : public BackoffGenerator {
  virtual ContextsLexer::Context
    operator()(const ContextsLexer::Context& c) { 
      if (c.size() <= 3)
        return ContextsLexer::Context();
      return strip_edges(c); 
    }
};


////////////////////////////////////////////////////////////////
// ContextsCorpus
////////////////////////////////////////////////////////////////

class ContextsCorpus : public Corpus {
  friend void read_callback(const ContextsLexer::PhraseContextsType&, void*);

public:
    typedef boost::ptr_vector<Document>::const_iterator const_iterator;

public:
    ContextsCorpus() : m_backoff(new TermBackoff) {}
    virtual ~ContextsCorpus() {}

    unsigned read_contexts(const std::string &filename, 
                           BackoffGenerator* backoff_gen=0);

    TermBackoffPtr backoff_index() {
      return m_backoff;
    }

private:
    TermBackoffPtr m_backoff;
    Dict m_dict;
};

#endif // _CONTEXTS_CORPUS_HH
