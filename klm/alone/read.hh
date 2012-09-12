#ifndef ALONE_READ__
#define ALONE_READ__

#include "util/exception.hh"

#include <iosfwd>

namespace util { class FilePiece; }

namespace search { template <class Model> class Context; }

namespace alone {

class Graph;
class Vocab;

class FormatException : public util::Exception {
  public:
    FormatException() {}
    ~FormatException() throw() {}
};

void JustVocab(util::FilePiece &from, std::ostream &to);

template <class Model> bool ReadCDec(search::Context<Model> &context, util::FilePiece &from, Graph &to, Vocab &vocab);

} // namespace alone

#endif // ALONE_READ__
