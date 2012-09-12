#ifndef ALONE_LABELED_EDGE__
#define ALONE_LABELED_EDGE__

#include "search/edge.hh"

#include <string>
#include <vector>

namespace alone {

class LabeledEdge : public search::Edge {
  public:
    LabeledEdge() {}

    void AppendWord(const std::string *word) {
      words_.push_back(word);
    }

    const std::vector<const std::string *> &Words() const {
      return words_;
    }

  private:
    // NULL for non-terminals.  
    std::vector<const std::string*> words_;
};

} // namespace alone

#endif // ALONE_LABELED_EDGE__
