#include <sstream>
#include <iostream>
#include <set>

#include "corpus.hh"
#include "gzstream.hh"

using namespace std;

//////////////////////////////////////////////////
// Corpus
//////////////////////////////////////////////////

Corpus::Corpus() : m_num_terms(0), m_num_types(0) {}

unsigned Corpus::read(const std::string &filename) {
  m_num_terms = 0;
  m_num_types = 0;
  std::set<int> seen_types;

  igzstream in(filename.c_str());

  string buf;
  int token;
  unsigned doc_count=0;
  while (getline(in, buf)) {
    Document* doc(new Document());
    istringstream ss(buf);

    ss >> token; // the number of unique terms

    char delimeter;
    int count;
    while(ss >> token >> delimeter >> count) {
      for (int i=0; i<count; ++i)
        doc->push_back(token);
      m_num_terms += count;
      seen_types.insert(token);
    }

    m_documents.push_back(doc);
    doc_count++;
  }

  m_num_types = seen_types.size();

  return doc_count;
}


//////////////////////////////////////////////////
// TestCorpus
//////////////////////////////////////////////////

TestCorpus::TestCorpus() {}

void TestCorpus::read(const std::string &filename) {
  igzstream in(filename.c_str());

  string buf;
  Term term;
  DocumentId doc;
  char delimeter;
  while (getline(in, buf)) {
    DocumentTerms* line(new DocumentTerms());
    istringstream ss(buf);

    while(ss >> doc >> delimeter >> term)
      line->push_back(DocumentTerm(doc, term));

    m_lines.push_back(line);
  }
}

//////////////////////////////////////////////////
// TermBackoff
//////////////////////////////////////////////////

void TermBackoff::read(const std::string &filename) {
  igzstream in(filename.c_str());

  string buf;
  int num_terms;
  getline(in, buf);
  istringstream ss(buf); 
  ss >> num_terms >> m_backoff_order;

  m_dict.resize(num_terms, -1);
  for (int i=0; i<m_backoff_order; ++i) {
    int count; ss >> count;
    m_terms_at_order.push_back(count);
  }

  Term term, backoff;
  while (getline(in, buf)) {
    istringstream ss(buf);
    ss >> term >> backoff;

    assert(term < num_terms);
    assert(term >= 0);

    m_dict[term] = backoff;
  }
}
