#include "alone/read.hh"

#include "alone/graph.hh"
#include "alone/vocab.hh"
#include "search/arity.hh"
#include "search/context.hh"
#include "search/weights.hh"
#include "util/file_piece.hh"

#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

#include <cstdlib>

namespace alone {

namespace {

template <class Model> Graph::Edge &ReadEdge(search::Context<Model> &context, util::FilePiece &from, Graph &to, Vocab &vocab, bool final) {
  Graph::Edge *ret = to.NewEdge();

  StringPiece got;

  std::vector<lm::WordIndex> words;
  unsigned long int terminals = 0;
  while ("|||" != (got = from.ReadDelimited())) {
    if ('[' == *got.data() && ']' == got.data()[got.size() - 1]) {
      // non-terminal
      char *end_ptr;
      unsigned long int child = std::strtoul(got.data() + 1, &end_ptr, 10);
      UTIL_THROW_IF(end_ptr != got.data() + got.size() - 1, FormatException, "Bad non-terminal" << got);
      UTIL_THROW_IF(child >= to.VertexSize(), FormatException, "Reference to vertex " << child << " but we only have " << to.VertexSize() << " vertices.  Is the file in bottom-up format?");
      ret->Add(to.MutableVertex(child));
      words.push_back(lm::kMaxWordIndex);
      ret->AppendWord(NULL);
    } else {
      const std::pair<const std::string, lm::WordIndex> &found = vocab.FindOrAdd(got);
      words.push_back(found.second);
      ret->AppendWord(&found.first);
      ++terminals;
    }
  }
  if (final) {
    // This is not counted for the word penalty.  
    words.push_back(vocab.EndSentence().second);
    ret->AppendWord(&vocab.EndSentence().first);
  }
  // Hard-coded word penalty.  
  float additive = context.GetWeights().DotNoLM(from.ReadLine()) - context.GetWeights().WordPenalty() * static_cast<float>(terminals) / M_LN10;
  ret->InitRule().Init(context, additive, words, final);
  unsigned int arity = ret->GetRule().Arity();
  UTIL_THROW_IF(arity > search::kMaxArity, util::Exception, "Edit search/arity.hh and increase " << search::kMaxArity << " to at least " << arity);
  return *ret;
}

} // namespace

// TODO: refactor
void JustVocab(util::FilePiece &from, std::ostream &out) {
  boost::unordered_set<std::string> seen;
  unsigned long int vertices = from.ReadULong();
  from.ReadULong(); // edges
  UTIL_THROW_IF(vertices == 0, FormatException, "Vertex count is zero");
  UTIL_THROW_IF('\n' != from.get(), FormatException, "Expected newline after counts");
  std::string temp;
  for (unsigned long int i = 0; i < vertices; ++i) {
    unsigned long int edge_count = from.ReadULong();
    UTIL_THROW_IF('\n' != from.get(), FormatException, "Expected after edge count");
    for (unsigned long int e = 0; e < edge_count; ++e) {
      StringPiece got;
      while ("|||" != (got = from.ReadDelimited())) {
        if ('[' == *got.data() && ']' == got.data()[got.size() - 1]) continue;
        temp.assign(got.data(), got.size());
        if (seen.insert(temp).second) out << temp << ' ';
      }
      from.ReadLine(); // weights
    }
  }
  // Eat sentence
  from.ReadLine();
}

template <class Model> bool ReadCDec(search::Context<Model> &context, util::FilePiece &from, Graph &to, Vocab &vocab) {
  unsigned long int vertices;
  try {
    vertices = from.ReadULong();
  } catch (const util::EndOfFileException &e) { return false; }
  unsigned long int edges = from.ReadULong();
  UTIL_THROW_IF(vertices < 2, FormatException, "Vertex count is " << vertices);
  UTIL_THROW_IF(edges == 0, FormatException, "Edge count is " << edges);
  --vertices;
  --edges;
  UTIL_THROW_IF('\n' != from.get(), FormatException, "Expected newline after counts");
  to.SetCounts(vertices, edges);
  Graph::Vertex *vertex;
  for (unsigned long int i = 0; ; ++i) {
    vertex = to.NewVertex();
    unsigned long int edge_count = from.ReadULong();
    bool root = (i == vertices - 1);
    UTIL_THROW_IF('\n' != from.get(), FormatException, "Expected after edge count");
    for (unsigned long int e = 0; e < edge_count; ++e) {
      vertex->Add(ReadEdge(context, from, to, vocab, root));
    }
    vertex->FinishedAdding();
    if (root) break;
  }
  to.SetRoot(vertex);
  StringPiece str = from.ReadLine();
  UTIL_THROW_IF("1" != str, FormatException, "Expected one edge to root");
  // The edge
  from.ReadLine();
  return true;
}

template bool ReadCDec(search::Context<lm::ngram::ProbingModel> &context, util::FilePiece &from, Graph &to, Vocab &vocab);
template bool ReadCDec(search::Context<lm::ngram::RestProbingModel> &context, util::FilePiece &from, Graph &to, Vocab &vocab);

} // namespace alone
