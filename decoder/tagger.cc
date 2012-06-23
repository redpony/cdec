#include "tagger.h"

#include "tdict.h"
#include "hg_io.h"
#include "filelib.h"
#include "hg.h"
#include "wordid.h"
#include "sentence_metadata.h"

using namespace std;

// This is a really simple linear chain tagger.
// You specify a tagset, and it hypothesizes that each word in the
// input can be tagged with any member of the tagset.
// The are a couple sample features implemented in ff_tagger.h/cc
// One thing to note, that while CRFs typically define the label
// sequence as corresponding to the hidden states in a trellis,
// in our model the labels are on edges, but mathematically
// they are identical.
//
// Things to do if you want to make this a "real" tagger:
// - support dictionaries (for each word, limit the tags considered)
// - add latent variables - this is really easy to do

static void ReadTagset(const string& file, vector<WordID>* tags) {
  ReadFile rf(file);
  istream& in(*rf.stream());
  while(in) {
    string tag;
    in >> tag;
    if (tag.empty()) continue;
    tags->push_back(TD::Convert(tag));
  }
  cerr << "Read " << tags->size() << " labels (tags) from " << file << endl;
}

struct TaggerImpl {
  TaggerImpl(const boost::program_options::variables_map& conf) :
      kXCAT(TD::Convert("X")*-1),
      kNULL(TD::Convert("<eps>")),
      kBINARY(new TRule("[X] ||| [X,1] [X,2] ||| [1] [2]")),
      kGOAL_RULE(new TRule("[Goal] ||| [X,1] ||| [1]")) {
    if (conf.count("tagger_tagset") == 0) {
      cerr << "Tagger requires --tagger_tagset FILE!\n";
      exit(1);
    }
    ReadTagset(conf["tagger_tagset"].as<string>(), &tagset_);
  }

  void BuildTrellis(const vector<WordID>& seq, Hypergraph* forest) {
    int prev_node_id = -1;
    for (int i = 0; i < seq.size(); ++i) {
      const WordID& src = seq[i];
      const int new_node_id = forest->AddNode(kXCAT)->id_;
      for (int k = 0; k < tagset_.size(); ++k) {
        TRulePtr rule(TRule::CreateLexicalRule(src, tagset_[k]));
        rule->lhs_ = kXCAT;
        Hypergraph::Edge* edge = forest->AddEdge(rule, Hypergraph::TailNodeVector());
        edge->i_ = i;
        edge->j_ = i+1;
        edge->prev_i_ = i;    // we set these for FastLinearIntersect
        edge->prev_j_ = i+1;  //      "      "            "
        forest->ConnectEdgeToHeadNode(edge->id_, new_node_id);
      }
      if (prev_node_id >= 0) {
        const int comb_node_id = forest->AddNode(kXCAT)->id_;
        Hypergraph::TailNodeVector tail(2, prev_node_id);
        tail[1] = new_node_id;
        Hypergraph::Edge* edge = forest->AddEdge(kBINARY, tail);
        edge->i_ = 0;
        edge->j_ = i+1;
        forest->ConnectEdgeToHeadNode(edge->id_, comb_node_id);
        prev_node_id = comb_node_id;
      } else {
        prev_node_id = new_node_id;
      }
    }
    Hypergraph::TailNodeVector tail(1, forest->nodes_.size() - 1);
    Hypergraph::Node* goal = forest->AddNode(TD::Convert("Goal")*-1);
    Hypergraph::Edge* hg_edge = forest->AddEdge(kGOAL_RULE, tail);
    forest->ConnectEdgeToHeadNode(hg_edge, goal);
  }

 private:
  vector<WordID> tagset_;
  const WordID kXCAT;
  const WordID kNULL;
  const TRulePtr kBINARY;
  const TRulePtr kGOAL_RULE;
};

Tagger::Tagger(const boost::program_options::variables_map& conf) :
 pimpl_(new TaggerImpl(conf)) {}


bool Tagger::TranslateImpl(const string& input,
                       SentenceMetadata* smeta,
                       const vector<double>& weights,
                       Hypergraph* forest) {
  Lattice& lattice = smeta->src_lattice_;
  LatticeTools::ConvertTextToLattice(input, &lattice);
  smeta->SetSourceLength(lattice.size());
  vector<WordID> sequence(lattice.size());
  for (int i = 0; i < lattice.size(); ++i) {
    assert(lattice[i].size() == 1);
    sequence[i] = lattice[i][0].label;
  }
  pimpl_->BuildTrellis(sequence, forest);
  forest->Reweight(weights);
  forest->is_linear_chain_ = true;
  return true;
}

