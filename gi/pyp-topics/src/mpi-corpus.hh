#ifndef _MPI_CORPUS_HH
#define _MPI_CORPUS_HH

#include <vector>
#include <string>
#include <map>
#include <tr1/unordered_map>

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>

#include "contexts_corpus.hh"


////////////////////////////////////////////////////////////////
// MPICorpus
////////////////////////////////////////////////////////////////

class MPICorpus : public ContextsCorpus {
public:
  MPICorpus() : ContextsCorpus() {
    boost::mpi::communicator world;
    m_rank = world.rank();
    m_size = world.size();
    m_start = -1;
    m_end = -1;
  }
  virtual ~MPICorpus() {}

  virtual unsigned read_contexts(const std::string &filename, 
                                 BackoffGenerator* backoff_gen=0,
                                 bool filter_singeltons=false,
                                 bool binary_contexts=false) {
    unsigned result = ContextsCorpus::read_contexts(filename, backoff_gen, filter_singeltons, binary_contexts);

    if (m_rank == 0) std::cerr << "\tLoad balancing terms per mpi segment:" << std::endl;
    float segment_size = num_terms() / m_size;
    float term_threshold = segment_size;
    int seen_terms = 0;
    std::vector<int> end_points;
    for (int i=0; i < num_documents(); ++i) {
      seen_terms += m_documents.at(i).size();
      if (seen_terms >= term_threshold) {
        end_points.push_back(i+1);
        term_threshold += segment_size;
        if (m_rank == 0) std::cerr << "\t\t" << i+1 << ": " <<  seen_terms << " terms, " << 100*seen_terms / (float)num_terms() << "%" << std::endl;
      }
    }
    m_start = (m_rank == 0 ? 0 : end_points.at(m_rank-1));
    m_end = (m_rank == m_size-1 ? num_documents() : end_points.at(m_rank));

    return result;
  }

  void
  bounds(int* start, int* end) const {
    *start = m_start;
    *end = m_end;
  }



protected:
  int m_rank, m_size;
  int m_start, m_end;
};

#endif // _MPI_CORPUS_HH
