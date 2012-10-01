#include "alone/threading.hh"
#include "search/config.hh"
#include "search/context.hh"
#include "util/exception.hh"
#include "util/file_piece.hh"
#include "util/usage.hh"

#include <boost/lexical_cast.hpp>

#include <iostream>
#include <memory>

namespace alone {

template <class Control> void ReadLoop(const std::string &graph_prefix, Control &control) {
  for (unsigned int sentence = 0; ; ++sentence) {
    std::stringstream name;
    name << graph_prefix << '/' << sentence;
    std::auto_ptr<util::FilePiece> file;
    try {
      file.reset(new util::FilePiece(name.str().c_str()));
    } catch (const util::ErrnoException &e) {
      if (e.Error() == ENOENT) return;
      throw;
    }
    control.Add(file.release());
  }
}

template <class Model> void RunWithModelType(const char *graph_prefix, const char *model_file, StringPiece weight_str, unsigned int pop_limit, unsigned int threads) {
  Model model(model_file);
  search::Weights weights(weight_str);
  search::Config config(weights, pop_limit);

  if (threads > 1) {
#ifdef WITH_THREADS
    Controller<Model> controller(config, model, threads, std::cout);
    ReadLoop(graph_prefix, controller);
#else
    UTIL_THROW(util::Exception, "Threading support not compiled in.");
#endif
  } else {
    InThread<Model> controller(config, model, std::cout);
    ReadLoop(graph_prefix, controller);
  }
}

void Run(const char *graph_prefix, const char *lm_name, StringPiece weight_str, unsigned int pop_limit, unsigned int threads) {
  lm::ngram::ModelType model_type;
  if (!lm::ngram::RecognizeBinary(lm_name, model_type)) model_type = lm::ngram::PROBING;
  switch (model_type) {
    case lm::ngram::PROBING:
      RunWithModelType<lm::ngram::ProbingModel>(graph_prefix, lm_name, weight_str, pop_limit, threads);
      break;
    case lm::ngram::REST_PROBING:
      RunWithModelType<lm::ngram::RestProbingModel>(graph_prefix, lm_name, weight_str, pop_limit, threads);
      break;
    default:
      UTIL_THROW(util::Exception, "Sorry this lm type isn't supported yet.");
  }
}

} // namespace alone

int main(int argc, char *argv[]) {
  if (argc < 5 || argc > 6) {
    std::cerr << argv[0] << " graph_prefix lm \"weights\" pop [threads]" << std::endl;
    return 1;
  }

#ifdef WITH_THREADS
  unsigned thread_count = boost::thread::hardware_concurrency();
#else
  unsigned thread_count = 1;
#endif
  if (argc == 6) {
    thread_count = boost::lexical_cast<unsigned>(argv[5]);
    UTIL_THROW_IF(!thread_count, util::Exception, "Thread count 0");
  }
  UTIL_THROW_IF(!thread_count, util::Exception, "Boost doesn't know how many threads there are.  Pass it on the command line.");
  alone::Run(argv[1], argv[2], argv[3], boost::lexical_cast<unsigned int>(argv[4]), thread_count);

  util::PrintUsage(std::cerr);
  return 0;
}
