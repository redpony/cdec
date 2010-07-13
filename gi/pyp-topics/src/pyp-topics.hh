#ifndef PYP_TOPICS_HH
#define PYP_TOPICS_HH

#include <vector>
#include <iostream>
#include <boost/ptr_container/ptr_vector.hpp>

#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random/mersenne_twister.hpp>

#include "pyp.hh"
#include "corpus.hh"
#include "workers.hh"

class PYPTopics {
public:
  typedef std::vector<int> DocumentTopics;
  typedef std::vector<DocumentTopics> CorpusTopics;
  typedef double F;

public:
  PYPTopics(int num_topics, bool use_topic_pyp=false, unsigned long seed = 0,
        int max_threads = 1, int num_jobs = 1) 
    : m_num_topics(num_topics), m_word_pyps(1), 
    m_topic_pyp(0.5,1.0,seed), m_use_topic_pyp(use_topic_pyp),
    m_seed(seed),
    uni_dist(0,1), rng(seed == 0 ? (unsigned long)this : seed), 
    rnd(rng, uni_dist), max_threads(max_threads), num_jobs(num_jobs) {}

  void sample_corpus(const Corpus& corpus, int samples,
                     int freq_cutoff_start=0, int freq_cutoff_end=0, 
                     int freq_cutoff_interval=0);

  int sample(const DocumentId& doc, const Term& term);
  std::pair<int,F> max(const DocumentId& doc, const Term& term) const;
  std::pair<int,F> max(const DocumentId& doc) const;
  int max_topic() const;

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
  typedef boost::ptr_vector< PYP<int> > PYPs;
  PYPs m_document_pyps;
  std::vector<PYPs> m_word_pyps;
  PYP<int> m_topic_pyp;
  bool m_use_topic_pyp;

  unsigned long m_seed;

  typedef boost::mt19937 base_generator_type;
  typedef boost::uniform_real<> uni_dist_type;
  typedef boost::variate_generator<base_generator_type&, uni_dist_type> gen_type;

  uni_dist_type uni_dist;
  base_generator_type rng; //this gets the seed
  gen_type rnd; //instantiate: rnd(rng, uni_dist)
                //call: rnd() generates uniform on [0,1)

  typedef boost::function<F()> JobReturnsF;

  F hresample_docs(int start, int end); //does i in [start, end)

  F hresample_topics();
  
  int max_threads;
  int num_jobs;
  TermBackoffPtr m_backoff;
};

#endif // PYP_TOPICS_HH
