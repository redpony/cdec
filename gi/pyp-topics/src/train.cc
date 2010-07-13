// STL
#include <iostream>
#include <fstream>

// Boost
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/scoped_ptr.hpp>

// Local
#include "pyp-topics.hh"
#include "corpus.hh"
#include "contexts_corpus.hh"
#include "gzstream.hh"

static const char *REVISION = "$Rev$";

// Namespaces
using namespace boost;
using namespace boost::program_options;
using namespace std;

int main(int argc, char **argv)
{
  std::cout << "Pitman Yor topic models: Copyright 2010 Phil Blunsom\n";
  std::cout << REVISION << '\n' << std::endl;

  ////////////////////////////////////////////////////////////////////////////////////////////
  // Command line processing
  variables_map vm; 

  // Command line processing
  options_description cmdline_specific("Command line specific options");
  cmdline_specific.add_options()
    ("help,h", "print help message")
    ("config,c", value<string>(), "config file specifying additional command line options")
    ;
  options_description generic("Allowed options");
  generic.add_options()
    ("documents,d", value<string>(), "file containing the documents")
    ("topics,t", value<int>()->default_value(50), "number of topics")
    ("document-topics-out,o", value<string>(), "file to write the document topics to")
    ("topic-words-out,w", value<string>(), "file to write the topic word distribution to")
    ("samples,s", value<int>()->default_value(10), "number of sampling passes through the data")
    ("test-corpus", value<string>(), "file containing the test data")
    ("backoff-paths", value<string>(), "file containing the term backoff paths")
    ;
  options_description config_options, cmdline_options;
  config_options.add(generic);
  cmdline_options.add(generic).add(cmdline_specific);

  store(parse_command_line(argc, argv, cmdline_options), vm); 
  if (vm.count("config") > 0) {
    ifstream config(vm["config"].as<string>().c_str());
    store(parse_config_file(config, cmdline_options), vm); 
  }
  notify(vm);
  ////////////////////////////////////////////////////////////////////////////////////////////

  if (vm.count("documents") == 0) {
    cerr << "Please specify a file containing the documents." << endl;
    cout << cmdline_options << "\n"; 
    return 1;
  }

  if (vm.count("help")) { 
    cout << cmdline_options << "\n"; 
    return 1; 
  }

  // seed the random number generator: 0 = automatic, specify value otherwise
  unsigned long seed = 0; 
  PYPTopics model(vm["topics"].as<int>(), false, seed);

  // read the data
  Corpus corpus;
  corpus.read(vm["documents"].as<string>());

  // read the backoff dictionary
  if (vm.count("backoff-paths"))
    model.set_backoff(vm["backoff-paths"].as<string>());

  // train the sampler
  model.sample_corpus(corpus, vm["samples"].as<int>());

  if (vm.count("document-topics-out")) {
    ogzstream documents_out(vm["document-topics-out"].as<string>().c_str());
    //model.print_document_topics(documents_out);

    int document_id=0;
    for (Corpus::const_iterator corpusIt=corpus.begin(); 
         corpusIt != corpus.end(); ++corpusIt, ++document_id) {
      std::vector<int> unique_terms;
      for (Document::const_iterator docIt=corpusIt->begin();
           docIt != corpusIt->end(); ++docIt) {
        if (unique_terms.empty() || *docIt != unique_terms.back())
          unique_terms.push_back(*docIt);
      }
      documents_out << unique_terms.size();
      for (std::vector<int>::const_iterator termIt=unique_terms.begin();
           termIt != unique_terms.end(); ++termIt)
        documents_out << " " << *termIt << ":" << model.max(document_id, *termIt).first;
      documents_out << std::endl;
    }
    documents_out.close();
  }

  if (vm.count("topic-words-out")) {
    ogzstream topics_out(vm["topic-words-out"].as<string>().c_str());
    model.print_topic_terms(topics_out);
    topics_out.close();
  }

  if (vm.count("test-corpus")) {
    TestCorpus test_corpus;
    test_corpus.read(vm["test-corpus"].as<string>());
    ogzstream topics_out((vm["test-corpus"].as<string>() + ".topics.gz").c_str());

    for (TestCorpus::const_iterator corpusIt=test_corpus.begin(); 
         corpusIt != test_corpus.end(); ++corpusIt) {
      int index=0;
      for (DocumentTerms::const_iterator instanceIt=corpusIt->begin();
           instanceIt != corpusIt->end(); ++instanceIt, ++index) {
        int topic = model.max(instanceIt->doc, instanceIt->term).first;
        if (index != 0) topics_out << " ";
        topics_out << topic;
      }
      topics_out << std::endl;
    }
    topics_out.close();
  }
  std::cout << std::endl;

  return 0;
}
