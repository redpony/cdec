#include "hg.h"
#include "lazy.h"
#include "fdict.h"
#include "tdict.h"

#include "lm/enumerate_vocab.hh"
#include "lm/model.hh"
#include "search/config.hh"
#include "search/context.hh"
#include "search/edge.hh"
#include "search/vertex.hh"
#include "search/vertex_generator.hh"
#include "util/exception.hh"

#include <boost/scoped_ptr.hpp>
#include <boost/scoped_array.hpp>

#include <iostream>
#include <vector>

namespace {

struct MapVocab : public lm::EnumerateVocab {
  public:
    MapVocab() {}

    // Do not call after Lookup.  
    void Add(lm::WordIndex index, const StringPiece &str) {
      const WordID cdec_id = TD::Convert(str.as_string());
      if (cdec_id >= out_.size()) out_.resize(cdec_id + 1);
      out_[cdec_id] = index;
    }

    // Assumes Add has been called and will never be called again.  
    lm::WordIndex FromCDec(WordID id) const {
      return out_[out_.size() > id ? id : 0];
    }

  private:
    std::vector<lm::WordIndex> out_;
};

class LazyBase {
  public:
    LazyBase(const std::vector<weight_t> &weights) : 
      cdec_weights_(weights),
      weights_(weights[FD::Convert("KLanguageModel")], weights[FD::Convert("KLanguageModel_OOV")], weights[FD::Convert("WordPenalty")]) {
      std::cerr << "Weights KLanguageModel " << weights_.LM() << " KLanguageModel_OOV " << weights_.OOV() << " WordPenalty " << weights_.WordPenalty() << std::endl;
    }

    virtual ~LazyBase() {}

    virtual void Search(unsigned int pop_limit, const Hypergraph &hg) const = 0;

    static LazyBase *Load(const char *model_file, const std::vector<weight_t> &weights);

  protected:
    lm::ngram::Config GetConfig() {
      lm::ngram::Config ret;
      ret.enumerate_vocab = &vocab_;
      return ret;
    }

    MapVocab vocab_;

    const std::vector<weight_t> &cdec_weights_;

    const search::Weights weights_;
};

template <class Model> class Lazy : public LazyBase {
  public:
    Lazy(const char *model_file, const std::vector<weight_t> &weights) : LazyBase(weights), m_(model_file, GetConfig()) {}

    void Search(unsigned int pop_limit, const Hypergraph &hg) const;

  private:
    void ConvertEdge(const search::Context<Model> &context, bool final, search::Vertex *vertices, const Hypergraph::Edge &in, search::Edge &out) const;

    const Model m_;
};

LazyBase *LazyBase::Load(const char *model_file, const std::vector<weight_t> &weights) {
  lm::ngram::ModelType model_type;
  if (!lm::ngram::RecognizeBinary(model_file, model_type)) model_type = lm::ngram::PROBING;
  switch (model_type) {
    case lm::ngram::PROBING:
      return new Lazy<lm::ngram::ProbingModel>(model_file, weights);
    case lm::ngram::REST_PROBING:
      return new Lazy<lm::ngram::RestProbingModel>(model_file, weights);
    default:
      UTIL_THROW(util::Exception, "Sorry this lm type isn't supported yet.");
  }
}

void PrintFinal(const Hypergraph &hg, const search::Edge *edge_base, const search::Final &final) {
  const std::vector<WordID> &words = hg.edges_[&final.From() - edge_base].rule_->e();
  boost::array<const search::Final*, search::kMaxArity>::const_iterator child(final.Children().begin());
  for (std::vector<WordID>::const_iterator i = words.begin(); i != words.end(); ++i) {
    if (*i > 0) {
      std::cout << TD::Convert(*i) << ' ';
    } else {
      PrintFinal(hg, edge_base, **child++);
    }
  }
}

template <class Model> void Lazy<Model>::Search(unsigned int pop_limit, const Hypergraph &hg) const {
  boost::scoped_array<search::Vertex> out_vertices(new search::Vertex[hg.nodes_.size()]);
  boost::scoped_array<search::Edge> out_edges(new search::Edge[hg.edges_.size()]);
  search::Config config(weights_, pop_limit);
  search::Context<Model> context(config, m_);

  for (unsigned int i = 0; i < hg.nodes_.size(); ++i) {
    search::Vertex &out_vertex = out_vertices[i];
    const Hypergraph::EdgesVector &down_edges = hg.nodes_[i].in_edges_;
    for (unsigned int j = 0; j < down_edges.size(); ++j) {
      unsigned int edge_index = down_edges[j];
      ConvertEdge(context, i == hg.nodes_.size() - 1, out_vertices.get(), hg.edges_[edge_index], out_edges[edge_index]);
      out_vertex.Add(out_edges[edge_index]);
    }
    out_vertex.FinishedAdding();
    search::VertexGenerator(context, out_vertex);
  }
  search::PartialVertex top = out_vertices[hg.nodes_.size() - 1].RootPartial(); 
  if (top.Empty()) {
    std::cout << "NO PATH FOUND";
  } else {
    search::PartialVertex continuation;
    while (!top.Complete()) {
      top.Split(continuation);
      top = continuation;
    }
    PrintFinal(hg, out_edges.get(), top.End());
    std::cout << "||| " << top.End().Bound() << std::endl;
  }
}

// TODO: get weights into here somehow.  
template <class Model> void Lazy<Model>::ConvertEdge(const search::Context<Model> &context, bool final, search::Vertex *vertices, const Hypergraph::Edge &in, search::Edge &out) const {
  const std::vector<WordID> &e = in.rule_->e();
  std::vector<lm::WordIndex> words;
  unsigned int terminals = 0;
  for (std::vector<WordID>::const_iterator word = e.begin(); word != e.end(); ++word) {
    if (*word <= 0) {
      out.Add(vertices[in.tail_nodes_[-*word]]);
      words.push_back(lm::kMaxWordIndex);
    } else {
      ++terminals;
      words.push_back(vocab_.FromCDec(*word));
    }
  }

  if (final) {
    words.push_back(m_.GetVocabulary().EndSentence());
  }

  float additive = in.rule_->GetFeatureValues().dot(cdec_weights_);
  UTIL_THROW_IF(isnan(additive), util::Exception, "Bad dot product");
  additive -= static_cast<float>(terminals) * context.GetWeights().WordPenalty() / M_LN10;

  out.InitRule().Init(context, additive, words, final);
}

boost::scoped_ptr<LazyBase> AwfulGlobalLazy;

} // namespace

void PassToLazy(const char *model_file, const std::vector<weight_t> &weights, unsigned int pop_limit, const Hypergraph &hg) {
  if (!AwfulGlobalLazy.get()) {
    AwfulGlobalLazy.reset(LazyBase::Load(model_file, weights));
  }
  AwfulGlobalLazy->Search(pop_limit, hg);
}
