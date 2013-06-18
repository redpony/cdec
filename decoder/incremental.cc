#include "incremental.h"

#include "hg.h"
#include "fdict.h"
#include "tdict.h"

#include "lm/enumerate_vocab.hh"
#include "lm/model.hh"
#include "search/applied.hh"
#include "search/config.hh"
#include "search/context.hh"
#include "search/edge.hh"
#include "search/edge_generator.hh"
#include "search/rule.hh"
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

template <class Model> class Incremental : public IncrementalBase {
  public:
    Incremental(const char *model_file, const std::vector<weight_t> &weights) :
      IncrementalBase(weights), 
      m_(model_file, GetConfig()),
      lm_(weights[FD::Convert("KLanguageModel")]),
      oov_(weights[FD::Convert("KLanguageModel_OOV")]),
      word_penalty_(weights[FD::Convert("WordPenalty")]) {
      std::cerr << "Weights KLanguageModel " << lm_ << " KLanguageModel_OOV " << oov_ << " WordPenalty " << word_penalty_ << std::endl;
    }

    void Search(unsigned int pop_limit, const Hypergraph &hg) const;

  private:
    void ConvertEdge(const search::Context<Model> &context, search::Vertex *vertices, const Hypergraph::Edge &in, search::EdgeGenerator &gen) const;

    lm::ngram::Config GetConfig() {
      lm::ngram::Config ret;
      ret.enumerate_vocab = &vocab_;
      return ret;
    }

    MapVocab vocab_;

    const Model m_;

    const float lm_, oov_, word_penalty_;
};

void PrintApplied(const Hypergraph &hg, const search::Applied final) {
  const std::vector<WordID> &words = static_cast<const Hypergraph::Edge*>(final.GetNote().vp)->rule_->e();
  const search::Applied *child(final.Children());
  for (std::vector<WordID>::const_iterator i = words.begin(); i != words.end(); ++i) {
    if (*i > 0) {
      std::cout << TD::Convert(*i) << ' ';
    } else {
      PrintApplied(hg, *child++);
    }
  }
}

template <class Model> void Incremental<Model>::Search(unsigned int pop_limit, const Hypergraph &hg) const {
  boost::scoped_array<search::Vertex> out_vertices(new search::Vertex[hg.nodes_.size()]);
  search::Config config(lm_, pop_limit, search::NBestConfig(1));
  search::Context<Model> context(config, m_);
  search::SingleBest best;

  for (unsigned int i = 0; i < hg.nodes_.size() - 1; ++i) {
    search::EdgeGenerator gen;
    const Hypergraph::EdgesVector &down_edges = hg.nodes_[i].in_edges_;
    for (unsigned int j = 0; j < down_edges.size(); ++j) {
      unsigned int edge_index = down_edges[j];
      ConvertEdge(context, out_vertices.get(), hg.edges_[edge_index], gen);
    }
    search::VertexGenerator<search::SingleBest> vertex_gen(context, out_vertices[i], best);
    gen.Search(context, vertex_gen);
  }
  const search::Applied top = out_vertices[hg.nodes_.size() - 2].BestChild();
  if (!top.Valid()) {
    std::cout << "NO PATH FOUND" << std::endl;
  } else {
    PrintApplied(hg, top);
    std::cout << "||| " << top.GetScore() << std::endl;
  }
}

template <class Model> void Incremental<Model>::ConvertEdge(const search::Context<Model> &context, search::Vertex *vertices, const Hypergraph::Edge &in, search::EdgeGenerator &gen) const {
  const std::vector<WordID> &e = in.rule_->e();
  std::vector<lm::WordIndex> words;
  words.reserve(e.size());
  std::vector<search::PartialVertex> nts;
  unsigned int terminals = 0;
  float score = 0.0;
  for (std::vector<WordID>::const_iterator word = e.begin(); word != e.end(); ++word) {
    if (*word <= 0) {
      nts.push_back(vertices[in.tail_nodes_[-*word]].RootAlternate());
      if (nts.back().Empty()) return;
      score += nts.back().Bound();
      words.push_back(lm::kMaxWordIndex);
    } else {
      ++terminals;
      words.push_back(vocab_.FromCDec(*word));
    }
  }

  search::PartialEdge out(gen.AllocateEdge(nts.size()));

  memcpy(out.NT(), &nts[0], sizeof(search::PartialVertex) * nts.size());

  search::Note note;
  note.vp = &in;
  out.SetNote(note);

  score += in.rule_->GetFeatureValues().dot(cdec_weights_);
  score -= static_cast<float>(terminals) * word_penalty_ / M_LN10;
  search::ScoreRuleRet res(search::ScoreRule(context.LanguageModel(), words, out.Between()));
  score += res.prob * lm_ + static_cast<float>(res.oov) * oov_;

  out.SetScore(score);

  gen.AddEdge(out);
}

} // namespace

IncrementalBase *IncrementalBase::Load(const char *model_file, const std::vector<weight_t> &weights) {
  lm::ngram::ModelType model_type;
  if (!lm::ngram::RecognizeBinary(model_file, model_type)) model_type = lm::ngram::PROBING;
  switch (model_type) {
    case lm::ngram::PROBING:
      return new Incremental<lm::ngram::ProbingModel>(model_file, weights);
    case lm::ngram::REST_PROBING:
      return new Incremental<lm::ngram::RestProbingModel>(model_file, weights);
    default:
      UTIL_THROW(util::Exception, "Sorry this lm type isn't supported yet.");
  }
}

IncrementalBase::~IncrementalBase() {}

IncrementalBase::IncrementalBase(const std::vector<weight_t> &weights) : cdec_weights_(weights) {}
