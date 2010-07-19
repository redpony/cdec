#ifndef MPI_PYP_TOPICS_HH
#define MPI_PYP_TOPICS_HH

#include <vector>
#include <iostream>

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/inversive_congruential.hpp>
#include <boost/random/linear_congruential.hpp>
#include <boost/random/lagged_fibonacci.hpp>
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>


#include "mpi-pyp.hh"
#include "corpus.hh"

class MPIPYPTopics {
public:
  typedef std::vector<int> DocumentTopics;
  typedef std::vector<DocumentTopics> CorpusTopics;
  typedef double F;

public:
  MPIPYPTopics(int num_topics, bool use_topic_pyp=false, unsigned long seed = 0) 
    : m_num_topics(num_topics), m_word_pyps(1), 
    m_topic_pyp(0.5,1.0), m_use_topic_pyp(use_topic_pyp),
    m_seed(seed),
    uni_dist(0,1), rng(seed == 0 ? (unsigned long)this : seed), 
    rnd(rng, uni_dist), m_mpi_start(-1), m_mpi_end(-1) {
      boost::mpi::communicator m_world;
      m_rank = m_world.rank(); 
      m_size = m_world.size();
      m_am_root = (m_rank == 0);
    }

  void sample_corpus(const Corpus& corpus, int samples,
                     int freq_cutoff_start=0, int freq_cutoff_end=0, 
                     int freq_cutoff_interval=0,
                     int max_contexts_per_document=0);

  int sample(const DocumentId& doc, const Term& term);
  int max(const DocumentId& doc, const Term& term) const;
  int max(const DocumentId& doc) const;
  int max_topic() const;

  void set_backoff(const std::string& filename) {
    m_backoff.reset(new TermBackoff);
    m_backoff->read(filename);
    m_word_pyps.clear();
    m_word_pyps.resize(m_backoff->order(), MPIPYPs());
  }
  void set_backoff(TermBackoffPtr backoff) {
    m_backoff = backoff;
    m_word_pyps.clear();
    m_word_pyps.resize(m_backoff->order(), MPIPYPs());
  }

  F prob(const Term& term, int topic, int level=0) const;
  void decrement(const Term& term, int topic, int level=0);
  void increment(const Term& term, int topic, int level=0);

  std::ostream& print_document_topics(std::ostream& out) const;
  std::ostream& print_topic_terms(std::ostream& out) const;

  void synchronise();

private:
  F word_pyps_p0(const Term& term, int topic, int level) const;

  int m_num_topics;
  F m_term_p0, m_topic_p0, m_backoff_p0;

  CorpusTopics m_corpus_topics;
  typedef boost::ptr_vector< PYP<int> > PYPs;
  typedef boost::ptr_vector< MPIPYP<int> > MPIPYPs;
  PYPs m_document_pyps;
  std::vector<MPIPYPs> m_word_pyps;
  MPIPYP<int> m_topic_pyp;
  bool m_use_topic_pyp;

  unsigned long m_seed;

  //typedef boost::mt19937 base_generator_type;
  //typedef boost::hellekalek1995 base_generator_type;
  typedef boost::lagged_fibonacci607 base_generator_type;
  typedef boost::uniform_real<> uni_dist_type;
  typedef boost::variate_generator<base_generator_type&, uni_dist_type> gen_type;

  uni_dist_type uni_dist;
  base_generator_type rng; //this gets the seed
  gen_type rnd; //instantiate: rnd(rng, uni_dist)
                //call: rnd() generates uniform on [0,1)

  TermBackoffPtr m_backoff;

  boost::mpi::communicator m_world;
  bool m_am_root;
  int m_rank, m_size;
  int m_mpi_start, m_mpi_end;
};

#endif // PYP_TOPICS_HH
