#include <sstream>
#include <iostream>
#include <set>

#include "contexts_corpus.hh"
#include "gzstream.hh"
#include "contexts_lexer.h"

using namespace std;

//////////////////////////////////////////////////
// ContextsCorpus
//////////////////////////////////////////////////

void read_callback(const ContextsLexer::PhraseContextsType& new_contexts, void* extra) {
  assert(new_contexts.contexts.size() == new_contexts.counts.size());

  ContextsCorpus* corpus_ptr = static_cast<ContextsCorpus*>(extra);
  Document* doc(new Document());

  //std::cout << "READ: " << new_contexts.phrase << "\t";

  for (int i=0; i < new_contexts.contexts.size(); ++i) {
    std::string context_str = "";
    for (ContextsLexer::Context::const_iterator it=new_contexts.contexts[i].begin();
         it != new_contexts.contexts[i].end(); ++it) {
      //std::cout << *it << " ";
      if (it != new_contexts.contexts[i].begin())
        context_str += "__";
      context_str += *it;
    }

    WordID id = corpus_ptr->m_dict.Convert(context_str);
    int count = new_contexts.counts[i];
    for (int i=0; i<count; ++i)
      doc->push_back(id);
    corpus_ptr->m_num_terms += count;

    //std::cout << context_str << " (" << id << ") ||| C=" << count << " ||| ";
  }
  //std::cout << std::endl;

  corpus_ptr->m_documents.push_back(doc);
}

unsigned ContextsCorpus::read_contexts(const std::string &filename) {
  m_num_terms = 0;
  m_num_types = 0;

  igzstream in(filename.c_str());
  ContextsLexer::ReadContexts(&in, read_callback, this);

  m_num_types = m_dict.max();

  return m_documents.size();
}
