#ifndef HG_TEST_H
#define HG_TEST_H

#include "filelib.h"
#include "hg.h"
#include "hg_io.h"
#include <sstream>

#pragma GCC diagnostic ignored "-Wunused-variable"

namespace {

typedef char const* Name;

Name urdu_json="urdu.json.gz";
Name urdu_wts="Arity_0 1.70741473606976 Arity_1 1.12426238048012 Arity_2 1.14986187839554 Glue -0.04589037041388 LanguageModel 1.09051 PassThrough -3.66226367902928 PhraseModel_0 -1.94633451863252 PhraseModel_1 -0.1475347695476 PhraseModel_2 -1.614818994946 WordPenalty -3.0 WordPenaltyFsa -0.56028442964748 ShorterThanPrev -10 LongerThanPrev -10";
Name small_json="small.json.gz";
Name small_wts="Model_0 -2 Model_1 -.5 Model_2 -1.1 Model_3 -1 Model_4 -1 Model_5 .5 Model_6 .2 Model_7 -.3";
Name perro_json="perro.json.gz";
Name perro_wts="SameFirstLetter 1 LongerThanPrev 1 ShorterThanPrev 1 GlueTop 0.0 Glue -1.0 EgivenF -0.5 FgivenE -0.5 LexEgivenF -0.5 LexFgivenE -0.5 LM 1";

}

// you can inherit from this or just use the static methods
struct HGSetup {
  static void CreateHG(const std::string& path,Hypergraph* hg);
  static void CreateHG_int(const std::string& path,Hypergraph* hg);
  static void CreateHG_tiny(const std::string& path, Hypergraph* hg);
  static void CreateHGBalanced(const std::string& path,Hypergraph* hg);
  static void CreateLatticeHG(const std::string& path,Hypergraph* hg);
  static void CreateTinyLatticeHG(const std::string& path,Hypergraph* hg);

  static void JsonFile(Hypergraph *hg,std::string f) {
    ReadFile rf(f);
    HypergraphIO::ReadFromJSON(rf.stream(), hg);
  }
  static void JsonTestFile(Hypergraph *hg,std::string path,std::string n) {
    JsonFile(hg,path + "/"+n);
  }
  static void CreateSmallHG(Hypergraph *hg, std::string path) { JsonTestFile(hg,path,small_json); }
};

void AddNullEdge(Hypergraph* hg) {
  TRule x;
  x.arity_ = 0;
  hg->nodes_[0].in_edges_.push_back(hg->AddEdge(TRulePtr(new TRule(x)), Hypergraph::TailNodeVector())->id_);
  hg->edges_.back().head_node_ = 0;
}

void HGSetup::CreateTinyLatticeHG(const std::string& path,Hypergraph* hg) {
  ReadFile rf(path + "/hg_test.tiny_lattice");
  HypergraphIO::ReadFromJSON(rf.stream(), hg);
  AddNullEdge(hg);
}

void HGSetup::CreateLatticeHG(const std::string& path,Hypergraph* hg) {
  ReadFile rf(path + "/hg_test.lattice");
  HypergraphIO::ReadFromJSON(rf.stream(), hg);
  AddNullEdge(hg);
}

void HGSetup::CreateHG_tiny(const std::string& path, Hypergraph* hg) {
  ReadFile rf(path + "/hg_test.tiny");
  HypergraphIO::ReadFromJSON(rf.stream(), hg);
}

void HGSetup::CreateHG_int(const std::string& path,Hypergraph* hg) {
  ReadFile rf(path + "/hg_test.hg_int");
  HypergraphIO::ReadFromJSON(rf.stream(), hg);
}

void HGSetup::CreateHG(const std::string& path,Hypergraph* hg) {
  ReadFile rf(path + "/hg_test.hg");
  HypergraphIO::ReadFromJSON(rf.stream(), hg);
}

void HGSetup::CreateHGBalanced(const std::string& path,Hypergraph* hg) {
  ReadFile rf(path + "/hg_test.hg_balanced");
  HypergraphIO::ReadFromJSON(rf.stream(), hg);
}

#endif
