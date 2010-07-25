#ifndef _CONTEXTS_CORPUS_HH
#define _CONTEXTS_CORPUS_HH

#include <vector>
#include <string>
#include <map>
#include <tr1/unordered_map>

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
    ContextsCorpus() : m_backoff(new TermBackoff) {}
    virtual ~ContextsCorpus() {}

    virtual unsigned read_contexts(const std::string &filename, 
                                   BackoffGenerator* backoff_gen=0,
                                   bool filter_singeltons=false,
                                   bool binary_contexts=false);

    TermBackoffPtr backoff_index() {
      return m_backoff;
    }

    std::vector<std::string> context2string(const WordID& id) const {
      std::vector<std::string> res;
      assert (id >= 0);
      m_dict.AsVector(id, &res);
      return res;
    }

    virtual int context_count(const WordID& id) const {
      return m_context_counts.find(id)->second;
    }


    const std::string& key(const int& i) const {
      return m_keys.at(i);
    }

protected:
    TermBackoffPtr m_backoff;
    Dict m_dict;
    std::vector<std::string> m_keys;
    std::tr1::unordered_map<int,int> m_context_counts;
};

#endif // _CONTEXTS_CORPUS_HH
