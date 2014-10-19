#ifndef HG_TEST_H
#define HG_TEST_H

#include "filelib.h"
#include "hg.h"
#include "hg_io.h"
#include <sstream>

#pragma GCC diagnostic ignored "-Wunused-variable"

namespace {

typedef char const* Name;

Name small_json="small.bin.gz";
Name small_wts="Model_0 -2 Model_1 -.5 Model_2 -1.1 Model_3 -1 Model_4 -1 Model_5 .5 Model_6 .2 Model_7 -.3";

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
    HypergraphIO::ReadFromBinary(rf.stream(), hg);
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
  ReadFile rf(path + "/hg_test.tiny_lattice.bin.gz");
  HypergraphIO::ReadFromBinary(rf.stream(), hg);
  AddNullEdge(hg);
}

void HGSetup::CreateLatticeHG(const std::string& path,Hypergraph* hg) {
  ReadFile rf(path + "/hg_test.lattice.bin.gz");
  HypergraphIO::ReadFromBinary(rf.stream(), hg);
  AddNullEdge(hg);
}

void HGSetup::CreateHG_tiny(const std::string& path, Hypergraph* hg) {
  ReadFile rf(path + "/hg_test.tiny.bin.gz");
  HypergraphIO::ReadFromBinary(rf.stream(), hg);
}

void HGSetup::CreateHG_int(const std::string& path,Hypergraph* hg) {
  ReadFile rf(path + "/hg_test.hg_int.bin.gz");
  HypergraphIO::ReadFromBinary(rf.stream(), hg);
}

void HGSetup::CreateHG(const std::string& path,Hypergraph* hg) {
  ReadFile rf(path + "/hg_test.hg.bin.gz");
  HypergraphIO::ReadFromBinary(rf.stream(), hg);
}

void HGSetup::CreateHGBalanced(const std::string& path,Hypergraph* hg) {
  ReadFile rf(path + "/hg_test.hg_balanced.bin.gz");
  HypergraphIO::ReadFromBinary(rf.stream(), hg);
}

#endif
