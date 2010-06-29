#ifndef _CORPUS_HH
#define _CORPUS_HH

#include <vector>
#include <string>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

////////////////////////////////////////////////////////////////
// Corpus
////////////////////////////////////////////////////////////////
typedef int Term;

typedef std::vector<Term> Document;
typedef std::vector<Term> Terms;

class Corpus {
public:
    typedef boost::ptr_vector<Document>::const_iterator const_iterator;

public:
    Corpus();
    virtual ~Corpus() {}

    unsigned read(const std::string &filename);

    const_iterator begin() const { return m_documents.begin(); }
    const_iterator end() const { return m_documents.end(); }

    const Document& at(size_t i) const { return m_documents.at(i); }

    int num_documents() const { return m_documents.size(); }
    int num_terms() const { return m_num_terms; }
    int num_types() const { return m_num_types; }

protected:
    int m_num_terms, m_num_types;
    boost::ptr_vector<Document> m_documents; 
};

typedef int DocumentId;
struct DocumentTerm {
  DocumentTerm(DocumentId d, Term t) : term(t), doc(d) {}
  Term term;
  DocumentId doc;
};
typedef std::vector<DocumentTerm> DocumentTerms;

class TestCorpus {
public:
    typedef boost::ptr_vector<DocumentTerms>::const_iterator const_iterator;

public:
    TestCorpus();
    ~TestCorpus() {}

    void read(const std::string &filename);

    const_iterator begin() const { return m_lines.begin(); }
    const_iterator end() const { return m_lines.end(); }

    int num_instances() const { return m_lines.size(); }

protected:
    boost::ptr_vector<DocumentTerms> m_lines; 
};

class TermBackoff {
public:
    typedef std::vector<Term> dictionary_type;
    typedef dictionary_type::const_iterator const_iterator;
    const static int NullBackoff=-1;

public:
    TermBackoff() { order(1); }
    ~TermBackoff() {}

    void read(const std::string &filename);

    const_iterator begin() const { return m_dict.begin(); }
    const_iterator end() const { return m_dict.end(); }

    const Term& operator[](const Term& t) const {
      assert(t < static_cast<int>(m_dict.size()));
      return m_dict[t];
    }

    Term& operator[](const Term& t) {
      if (t >= static_cast<int>(m_dict.size()))
        m_dict.resize(t+1, -1);
      return m_dict[t];
    }

    bool has_backoff(const Term& t) {
      return t >= 0 && t < static_cast<int>(m_dict.size()) && m_dict[t] >= 0;
    }

    int order() const { return m_backoff_order; }
    void order(int o) { 
      if (o >= (int)m_terms_at_order.size())
        m_terms_at_order.resize(o, 0);
      m_backoff_order = o; 
    }

//    int levels() const { return m_terms_at_order.size(); }
    bool is_null(const Term& term) const { return term < 0; }
    int terms_at_level(int level) const { 
      assert (level < (int)m_terms_at_order.size());
      return m_terms_at_order.at(level);
    }

    int& terms_at_level(int level) { 
      assert (level < (int)m_terms_at_order.size());
      return m_terms_at_order.at(level);
    }

    int size() const { return m_dict.size(); }

protected:
    dictionary_type m_dict; 
    int m_backoff_order;
    std::vector<int> m_terms_at_order;
};
typedef boost::shared_ptr<TermBackoff> TermBackoffPtr;

#endif // _CORPUS_HH
