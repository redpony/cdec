#include <sstream>
#include <iostream>
#include <set>

#include "contexts_corpus.hh"
#include "gzstream.hh"
#include "contexts_lexer.h"

#include <boost/tuple/tuple.hpp>


using namespace std;

//////////////////////////////////////////////////
// ContextsCorpus
//////////////////////////////////////////////////

void read_callback(const ContextsLexer::PhraseContextsType& new_contexts, void* extra) {
  assert(new_contexts.contexts.size() == new_contexts.counts.size());

  boost::tuple<ContextsCorpus*, BackoffGenerator*, map<string,int>* >* extra_pair
    = static_cast< boost::tuple<ContextsCorpus*, BackoffGenerator*, map<string,int>* >* >(extra);

  ContextsCorpus* corpus_ptr = extra_pair->get<0>();
  BackoffGenerator* backoff_gen = extra_pair->get<1>();
  //map<string,int>* counts = extra_pair->get<2>();

  Document* doc(new Document());

  //cout << "READ: " << new_contexts.phrase << "\t";
  for (int i=0; i < new_contexts.counts.size(); ++i) {
    int cache_word_count = corpus_ptr->m_dict.max();

    //string context_str = corpus_ptr->m_dict.toString(new_contexts.contexts[i]);
    int context_index = new_contexts.counts.at(i).first;
    string context_str = corpus_ptr->m_dict.toString(new_contexts.contexts[context_index]);

    // filter out singleton contexts
    //if (!counts->empty()) {
    //  map<string,int>::const_iterator find_it = counts->find(context_str);
    //  if (find_it == counts->end() || find_it->second < 2)
    //    continue;
    //}

    WordID id = corpus_ptr->m_dict.Convert(context_str);
    if (cache_word_count != corpus_ptr->m_dict.max()) {
      corpus_ptr->m_backoff->terms_at_level(0)++;
      corpus_ptr->m_num_types++;
    }

    //int count = new_contexts.counts[i];
    int count = new_contexts.counts.at(i).second;
    for (int j=0; j<count; ++j)
      doc->push_back(id);
    corpus_ptr->m_num_terms += count;

    // generate the backoff map
    if (backoff_gen) {
      int order = 1;
      WordID backoff_id = id;
      //ContextsLexer::Context backedoff_context = new_contexts.contexts[i];
      ContextsLexer::Context backedoff_context = new_contexts.contexts[context_index];
      while (true) {
        if (!corpus_ptr->m_backoff->has_backoff(backoff_id)) {
          //cerr << "Backing off from " << corpus_ptr->m_dict.Convert(backoff_id) << " to ";
          backedoff_context = (*backoff_gen)(backedoff_context);

          if (backedoff_context.empty()) {
            //cerr << "Nothing." << endl;
            (*corpus_ptr->m_backoff)[backoff_id] = -1;
            break;
          }

          if (++order > corpus_ptr->m_backoff->order())
            corpus_ptr->m_backoff->order(order);

          int cache_word_count = corpus_ptr->m_dict.max();
          int new_backoff_id = corpus_ptr->m_dict.Convert(backedoff_context);
          if (cache_word_count != corpus_ptr->m_dict.max())
            corpus_ptr->m_backoff->terms_at_level(order-1)++;

          //cerr << corpus_ptr->m_dict.Convert(new_backoff_id) << " ." << endl;

          backoff_id = ((*corpus_ptr->m_backoff)[backoff_id] = new_backoff_id);
        }
        else break;
      }
    }
    //cout << context_str << " (" << id << ") ||| C=" << count << " ||| ";
  }
  //cout << endl;

  //if (!doc->empty()) {
    corpus_ptr->m_documents.push_back(doc);
    corpus_ptr->m_keys.push_back(new_contexts.phrase);
  //}
}

void filter_callback(const ContextsLexer::PhraseContextsType& new_contexts, void* extra) {
  assert(new_contexts.contexts.size() == new_contexts.counts.size());

  map<string,int>* context_counts = (static_cast<map<string,int>*>(extra));

  for (int i=0; i < new_contexts.counts.size(); ++i) {
    int context_index = new_contexts.counts.at(i).first;
    int count = new_contexts.counts.at(i).second;
    //int count = new_contexts.counts[i];
    pair<map<string,int>::iterator,bool> result 
      = context_counts->insert(make_pair(Dict::toString(new_contexts.contexts[context_index]),count));
      //= context_counts->insert(make_pair(Dict::toString(new_contexts.contexts[i]),count));
    if (!result.second)
      result.first->second += count;
  }
}


unsigned ContextsCorpus::read_contexts(const string &filename, 
                                       BackoffGenerator* backoff_gen_ptr,
                                       bool /*filter_singeltons*/) {
  map<string,int> counts;
  //if (filter_singeltons) 
  {
  //  cerr << "--- Filtering singleton contexts ---" << endl;

    igzstream in(filename.c_str());
    ContextsLexer::ReadContexts(&in, filter_callback, &counts);
  }

  m_num_terms = 0;
  m_num_types = 0;

  igzstream in(filename.c_str());
  boost::tuple<ContextsCorpus*, BackoffGenerator*, map<string,int>* > extra_pair(this,backoff_gen_ptr,&counts);
  ContextsLexer::ReadContexts(&in, read_callback, &extra_pair);

  //m_num_types = m_dict.max();

  cerr << "Read backoff with order " << m_backoff->order() << "\n";
  for (int o=0; o<m_backoff->order(); o++)
    cerr << "  Terms at " << o << " = " << m_backoff->terms_at_level(o) << endl;
  //cerr << endl;

  int i=0; double av_freq=0;
  for (map<string,int>::const_iterator it=counts.begin(); it != counts.end(); ++it, ++i) {
    WordID id = m_dict.Convert(it->first);
    m_context_counts[id] = it->second;
    av_freq += it->second;
  }
  cerr << "  Average term frequency = " << av_freq / (double) i << endl;

  return m_documents.size();
}
