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

  std::pair<ContextsCorpus*, BackoffGenerator*>* extra_pair
    = static_cast< std::pair<ContextsCorpus*, BackoffGenerator*>* >(extra);

  ContextsCorpus* corpus_ptr = extra_pair->first;
  BackoffGenerator* backoff_gen = extra_pair->second;

  Document* doc(new Document());

  //std::cout << "READ: " << new_contexts.phrase << "\t";
  for (int i=0; i < new_contexts.contexts.size(); ++i) {
    int cache_word_count = corpus_ptr->m_dict.max();
    WordID id = corpus_ptr->m_dict.Convert(new_contexts.contexts[i]);
    if (cache_word_count != corpus_ptr->m_dict.max()) {
      corpus_ptr->m_backoff->terms_at_level(0)++;
      corpus_ptr->m_num_types++;
    }

    int count = new_contexts.counts[i];
    for (int j=0; j<count; ++j)
      doc->push_back(id);
    corpus_ptr->m_num_terms += count;

    // generate the backoff map
    if (backoff_gen) {
      int order = 1;
      WordID backoff_id = id;
      ContextsLexer::Context backedoff_context = new_contexts.contexts[i];
      while (true) {
        if (!corpus_ptr->m_backoff->has_backoff(backoff_id)) {
          //std::cerr << "Backing off from " << corpus_ptr->m_dict.Convert(backoff_id) << " to ";
          backedoff_context = (*backoff_gen)(backedoff_context);

          if (backedoff_context.empty()) {
            //std::cerr << "Nothing." << std::endl;
            (*corpus_ptr->m_backoff)[backoff_id] = -1;
            break;
          }

          if (++order > corpus_ptr->m_backoff->order())
            corpus_ptr->m_backoff->order(order);

          int cache_word_count = corpus_ptr->m_dict.max();
          int new_backoff_id = corpus_ptr->m_dict.Convert(backedoff_context);
          if (cache_word_count != corpus_ptr->m_dict.max())
            corpus_ptr->m_backoff->terms_at_level(order-1)++;

          //std::cerr << corpus_ptr->m_dict.Convert(new_backoff_id) << " ." << std::endl;

          backoff_id = ((*corpus_ptr->m_backoff)[backoff_id] = new_backoff_id);
        }
        else break;
      }
    }
    //std::cout << context_str << " (" << id << ") ||| C=" << count << " ||| ";
  }
  //std::cout << std::endl;

  corpus_ptr->m_documents.push_back(doc);
}

unsigned ContextsCorpus::read_contexts(const std::string &filename, 
                                       BackoffGenerator* backoff_gen_ptr) {
  m_num_terms = 0;
  m_num_types = 0;

  igzstream in(filename.c_str());
  std::pair<ContextsCorpus*, BackoffGenerator*> extra_pair(this,backoff_gen_ptr);
  ContextsLexer::ReadContexts(&in, 
                              read_callback, 
                              &extra_pair);

  //m_num_types = m_dict.max();

  std::cerr << "Read backoff with order " << m_backoff->order() << "\n";
  for (int o=0; o<m_backoff->order(); o++)
    std::cerr << "  Terms at " << o << " = " << m_backoff->terms_at_level(o) << std::endl;
  std::cerr << std::endl;

  return m_documents.size();
}
