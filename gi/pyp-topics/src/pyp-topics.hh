#ifndef PYP_TOPICS_HH
#define PYP_TOPICS_HH

#include <vector>
#include <iostream>

#include "pyp.hh"
#include "corpus.hh"


class PYPTopics {
public:
  typedef std::vector<int> DocumentTopics;
  typedef std::vector<DocumentTopics> CorpusTopics;
  typedef double F;

public:
  PYPTopics(int num_topics) : m_num_topics(num_topics), m_word_pyps(1) {}

  void sample(const Corpus& corpus, int samples);
  int sample(const DocumentId& doc, const Term& term);
  int max(const DocumentId& doc, const Term& term);

  void set_backoff(const std::string& filename) {
    m_backoff.reset(new TermBackoff);
    m_backoff->read(filename);
    m_word_pyps.clear();
    m_word_pyps.resize(m_backoff->order(), PYPs());
  }
  void set_backoff(TermBackoffPtr backoff) {
    m_backoff = backoff;
    m_word_pyps.clear();
    m_word_pyps.resize(m_backoff->order(), PYPs());
  }

  F prob(const Term& term, int topic, int level=0) const;
  void decrement(const Term& term, int topic, int level=0);
  void increment(const Term& term, int topic, int level=0);

  std::ostream& print_document_topics(std::ostream& out) const;
  std::ostream& print_topic_terms(std::ostream& out) const;

private:
  F word_pyps_p0(const Term& term, int topic, int level) const;

  int m_num_topics;
  F m_term_p0, m_topic_p0, m_backoff_p0;

  CorpusTopics m_corpus_topics;
  typedef std::vector< PYP<int> > PYPs;
  PYPs m_document_pyps;
  std::vector<PYPs> m_word_pyps;

  TermBackoffPtr m_backoff;
};

#endif // PYP_TOPICS_HH
