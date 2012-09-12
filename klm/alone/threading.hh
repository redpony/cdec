#ifndef ALONE_THREADING__
#define ALONE_THREADING__

#ifdef WITH_THREADS
#include "util/pcqueue.hh"
#include "util/pool.hh"
#endif

#include <iosfwd>
#include <queue>
#include <string>

namespace util {
class FilePiece;
} // namespace util

namespace search {
class Config;
template <class Model> class Context;
} // namespace search

namespace alone {

template <class Model> void Decode(const search::Config &config, const Model &model, util::FilePiece *in_ptr, std::ostream &out);

class Graph;

#ifdef WITH_THREADS
struct SentenceID {
  unsigned int sentence_id;
  bool operator==(const SentenceID &other) const {
    return sentence_id == other.sentence_id;
  }
};

struct Input : public SentenceID {
  util::FilePiece *file;
  static Input Poison() {
    Input ret;
    ret.sentence_id = static_cast<unsigned int>(-1);
    ret.file = NULL;
    return ret;
  }
};

struct Output : public SentenceID {
  std::string *str;
  static Output Poison() {
    Output ret;
    ret.sentence_id = static_cast<unsigned int>(-1);
    ret.str = NULL;
    return ret;
  }
};

template <class Model> class DecodeHandler {
  public:
    typedef Input Request;

    DecodeHandler(const search::Config &config, const Model &model, util::PCQueue<Output> &out) : config_(config), model_(model), out_(out) {}

    void operator()(Input message);

  private:
    void Produce(unsigned int sentence_id, const std::string &str);

    const search::Config &config_;

    const Model &model_;
    
    util::PCQueue<Output> &out_;
};

class PrintHandler {
  public:
    typedef Output Request;

    explicit PrintHandler(std::ostream &o) : out_(o), done_(0) {}

    void operator()(Output message);

  private:
    std::ostream &out_;
    std::deque<std::string*> waiting_;
    unsigned int done_;
};

template <class Model> class Controller {
  public:
    // This config must remain valid.   
    explicit Controller(const search::Config &config, const Model &model, size_t decode_workers, std::ostream &to);

    // Takes ownership of in.    
    void Add(util::FilePiece *in) {
      Input input;
      input.sentence_id = sentence_id_++;
      input.file = in;
      decoder_.Produce(input);
    }

  private:
    unsigned int sentence_id_;

    util::Pool<PrintHandler> printer_;

    util::Pool<DecodeHandler<Model> > decoder_;
};
#endif

// Same API as controller.  
template <class Model> class InThread {
  public:
    InThread(const search::Config &config, const Model &model, std::ostream &to) : config_(config), model_(model), to_(to) {}

    // Takes ownership of in.  
    void Add(util::FilePiece *in) {
      Decode(config_, model_, in, to_);
    }

  private:
    const search::Config &config_;

    const Model &model_;

    std::ostream &to_; 
};

} // namespace alone
#endif // ALONE_THREADING__
