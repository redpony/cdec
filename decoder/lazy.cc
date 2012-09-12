#include "hg.h"
#include "lazy.h"
#include "tdict.h"

#include "lm/enumerate_vocab.hh"
#include "lm/model.hh"
#include "search/edge.hh"
#include "search/vertex.hh"
#include "util/exception.hh"

#include <boost/scoped_array.hpp>

namespace {

struct MapVocab : public lm::EnumerateVocab {
  public:
    MapVocab() {}

    // Do not call after Lookup.  
    void Add(lm::WordIndex index, const StringPiece &str) {
      const WordID cdec_id = TD::Convert(str.as_string());
      if (cdec_id >= out_->size()) out_.resize(cdec_id + 1);
      out_[cdec_id] = index;
    }

    // Assumes Add has been called and will never be called again.  
    lm::WordIndex FromCDec(WordID id) const {
      return out_[out.size() > id ? id : 0];
    }

  private:
    std::vector<lm::WordIndex> out_;
};

class LazyBase {
  public:
    LazyBase() {}

    virtual ~LazyBase() {}

    virtual void Search(const Hypergraph &hg) const = 0;

    static LazyBase *Load(const char *model_file);

  protected:
    lm::ngram::Config GetConfig() const {
      lm::ngram::Config ret;
      ret.enumerate_vocab = &vocab_;
      return ret;
    }

    MapVocab vocab_;
};

template <class Model> class Lazy : public LazyBase {
  public:
    explicit Lazy(const char *model_file) : m_(model_file, GetConfig()) {}

    void Search(const Hypergraph &hg) const;

  private:
    void ConvertEdge(const Context<Model> &context, bool final, search::Vertex *vertices, const Hypergraph::Edge &in, search::Edge &out) const;

    const Model m_;
};

static LazyBase *LazyBase::Load(const char *model_file) {
  lm::ngram::ModelType model_type;
  if (!lm::ngram::RecognizeBinary(lm_name, model_type)) model_type = lm::ngram::PROBING;
  switch (model_type) {
    case lm::ngram::PROBING:
      return new Lazy<lm::ngram::ProbingModel>(model_file);
    case lm::ngram::REST_PROBING:
      return new Lazy<lm::ngram::RestProbingModel>(model_file);
    default:
      UTIL_THROW(util::Exception, "Sorry this lm type isn't supported yet.");
  }
}

template <class Model> void Lazy<Model>::Search(const Hypergraph &hg) const {
  boost::scoped_array<search::Vertex> out_vertices(new search::Vertex[hg.nodes_.size()]);
  boost::scoped_array<search::Edge> out_edges(new search::Edge[hg.edges_.size()]);
  for (unsigned int i = 0; i < hg.nodes_.size(); ++i) {
    search::Vertex *out_vertex = out_vertices[i];
    const Hypergraph::EdgesVector &down_edges = hg.nodes_[i].in_edges_;
    for (unsigned int j = 0; j < edges.size(); ++j) {
      unsigned int edge_index = down_edges[j];
      const Hypergraph::Edge &in_edge = hg.edges_[edge_index];
      search::Edge &out_edge = out_edges[edge_index];
    }
  }
}

// TODO: get weights into here somehow.  
template <class Model> void Lazy<Model>::ConvertEdge(const Context<Model> &context, bool final, search::Vertices *vertices, const Hypergraph::Edge &in, search::Edge &out) const {
  const std::vector<WordID> &e = in_edge.rule_->e();
  std::vector<lm::WordIndex> words;
  unsigned int terminals = 0;
  for (std::vector<WordID>::const_iterator word = e.begin(); word != e.end(); ++word) {
    if (*word <= 0) {
      out.Add(vertices[edge.tail_nodes_[-*word]]);
      words.push_back(lm::kMaxWordIndex);
    } else {
      ++terminals;
      words.push_back(vocab_.FromCDec(*word));
    }
  }

  if (final) {
    words.push_back(m_.GetVocabulary().EndSentence());
  }

  float additive = edge.rule_->GetFeatureValues().dot(weight_vector);

  out.InitRule().Init(context, additive, words, final);
}

} // namespace

void PassToLazy(const Hypergraph &hg) {

}
