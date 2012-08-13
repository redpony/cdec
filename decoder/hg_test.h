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
  enum {
    HG,
    HG_int,
    HG_tiny,
    HGBalanced,
    LatticeHG,
    TinyLatticeHG,
  };
  static void CreateHG(Hypergraph* hg);
  static void CreateHG_int(Hypergraph* hg);
  static void CreateHG_tiny(Hypergraph* hg);
  static void CreateHGBalanced(Hypergraph* hg);
  static void CreateLatticeHG(Hypergraph* hg);
  static void CreateTinyLatticeHG(Hypergraph* hg);

  static void Json(Hypergraph *hg,std::string const& json) {
    std::istringstream i(json);
    HypergraphIO::ReadFromJSON(&i, hg);
  }
  static void JsonFile(Hypergraph *hg,std::string f) {
    ReadFile rf(f);
    HypergraphIO::ReadFromJSON(rf.stream(), hg);
  }
  static void JsonTestFile(Hypergraph *hg,std::string path,std::string n) {
    JsonFile(hg,path + "/"+n);
  }
  static void CreateSmallHG(Hypergraph *hg, std::string path) { JsonTestFile(hg,path,small_json); }
};

namespace {
Name HGjsons[]= {
  "{\"rules\":[1,\"[X] ||| a\",2,\"[X] ||| A [1]\",3,\"[X] ||| c\",4,\"[X] ||| C [1]\",5,\"[X] ||| [1] B [2]\",6,\"[X] ||| [1] b [2]\",7,\"[X] ||| X [1]\",8,\"[X] ||| Z [1]\"],\"features\":[\"f1\",\"f2\",\"Feature_1\",\"Feature_0\",\"Model_0\",\"Model_1\",\"Model_2\",\"Model_3\",\"Model_4\",\"Model_5\",\"Model_6\",\"Model_7\"],\"edges\":[{\"tail\":[],\"feats\":[],\"rule\":1}],\"node\":{\"in_edges\":[0]},\"edges\":[{\"tail\":[0],\"feats\":[0,-0.8,1,-0.1],\"rule\":2}],\"node\":{\"in_edges\":[1]},\"edges\":[{\"tail\":[],\"feats\":[1,-1],\"rule\":3}],\"node\":{\"in_edges\":[2]},\"edges\":[{\"tail\":[2],\"feats\":[0,-0.2,1,-0.1],\"rule\":4}],\"node\":{\"in_edges\":[3]},\"edges\":[{\"tail\":[1,3],\"feats\":[0,-1.2,1,-0.2],\"rule\":5},{\"tail\":[1,3],\"feats\":[0,-0.5,1,-1.3],\"rule\":6}],\"node\":{\"in_edges\":[4,5]},\"edges\":[{\"tail\":[4],\"feats\":[0,-0.5,1,-0.8],\"rule\":7},{\"tail\":[4],\"feats\":[0,-0.7,1,-0.9],\"rule\":8}],\"node\":{\"in_edges\":[6,7]}}",
"{\"rules\":[1,\"[X] ||| a\",2,\"[X] ||| b\",3,\"[X] ||| a [1]\",4,\"[X] ||| [1] b\"],\"features\":[\"f1\",\"f2\",\"Feature_1\",\"Feature_0\",\"Model_0\",\"Model_1\",\"Model_2\",\"Model_3\",\"Model_4\",\"Model_5\",\"Model_6\",\"Model_7\"],\"edges\":[{\"tail\":[],\"feats\":[0,0.1],\"rule\":1},{\"tail\":[],\"feats\":[0,0.1],\"rule\":2}],\"node\":{\"in_edges\":[0,1],\"cat\":\"X\"},\"edges\":[{\"tail\":[0],\"feats\":[0,0.3],\"rule\":3},{\"tail\":[0],\"feats\":[0,0.2],\"rule\":4}],\"node\":{\"in_edges\":[2,3],\"cat\":\"Goal\"}}",
  "{\"rules\":[1,\"[X] ||| <s>\",2,\"[X] ||| X [1]\",3,\"[X] ||| Z [1]\"],\"features\":[\"f1\",\"f2\",\"Feature_1\",\"Feature_0\",\"Model_0\",\"Model_1\",\"Model_2\",\"Model_3\",\"Model_4\",\"Model_5\",\"Model_6\",\"Model_7\"],\"edges\":[{\"tail\":[],\"feats\":[0,-2,1,-99],\"rule\":1}],\"node\":{\"in_edges\":[0]},\"edges\":[{\"tail\":[0],\"feats\":[0,-0.5,1,-0.8],\"rule\":2},{\"tail\":[0],\"feats\":[0,-0.7,1,-0.9],\"rule\":3}],\"node\":{\"in_edges\":[1,2]}}",
  "{\"rules\":[1,\"[X] ||| i\",2,\"[X] ||| a\",3,\"[X] ||| b\",4,\"[X] ||| [1] [2]\",5,\"[X] ||| [1] [2]\",6,\"[X] ||| c\",7,\"[X] ||| d\",8,\"[X] ||| [1] [2]\",9,\"[X] ||| [1] [2]\",10,\"[X] ||| [1] [2]\",11,\"[X] ||| [1] [2]\",12,\"[X] ||| [1] [2]\",13,\"[X] ||| [1] [2]\"],\"features\":[\"f1\",\"f2\",\"Feature_1\",\"Feature_0\",\"Model_0\",\"Model_1\",\"Model_2\",\"Model_3\",\"Model_4\",\"Model_5\",\"Model_6\",\"Model_7\"],\"edges\":[{\"tail\":[],\"feats\":[],\"rule\":1}],\"node\":{\"in_edges\":[0]},\"edges\":[{\"tail\":[],\"feats\":[],\"rule\":2}],\"node\":{\"in_edges\":[1]},\"edges\":[{\"tail\":[],\"feats\":[],\"rule\":3}],\"node\":{\"in_edges\":[2]},\"edges\":[{\"tail\":[1,2],\"feats\":[],\"rule\":4},{\"tail\":[2,1],\"feats\":[],\"rule\":5}],\"node\":{\"in_edges\":[3,4]},\"edges\":[{\"tail\":[],\"feats\":[],\"rule\":6}],\"node\":{\"in_edges\":[5]},\"edges\":[{\"tail\":[],\"feats\":[],\"rule\":7}],\"node\":{\"in_edges\":[6]},\"edges\":[{\"tail\":[4,5],\"feats\":[],\"rule\":8},{\"tail\":[5,4],\"feats\":[],\"rule\":9}],\"node\":{\"in_edges\":[7,8]},\"edges\":[{\"tail\":[3,6],\"feats\":[],\"rule\":10},{\"tail\":[6,3],\"feats\":[],\"rule\":11}],\"node\":{\"in_edges\":[9,10]},\"edges\":[{\"tail\":[7,0],\"feats\":[],\"rule\":12},{\"tail\":[0,7],\"feats\":[],\"rule\":13}],\"node\":{\"in_edges\":[11,12]}}",
  "{\"rules\":[1,\"[X] ||| [1] a\",2,\"[X] ||| [1] A\",3,\"[X] ||| [1] A A\",4,\"[X] ||| [1] b\",5,\"[X] ||| [1] c\",6,\"[X] ||| [1] B C\",7,\"[X] ||| [1] A B C\",8,\"[X] ||| [1] CC\"],\"features\":[\"f1\",\"f2\",\"Feature_1\",\"Feature_0\",\"Model_0\",\"Model_1\",\"Model_2\",\"Model_3\",\"Model_4\",\"Model_5\",\"Model_6\",\"Model_7\"],\"edges\":[],\"node\":{\"in_edges\":[]},\"edges\":[{\"tail\":[0],\"feats\":[2,-0.3],\"rule\":1},{\"tail\":[0],\"feats\":[2,-0.6],\"rule\":2},{\"tail\":[0],\"feats\":[2,-1.7],\"rule\":3}],\"node\":{\"in_edges\":[0,1,2]},\"edges\":[{\"tail\":[1],\"feats\":[2,-0.5],\"rule\":4}],\"node\":{\"in_edges\":[3]},\"edges\":[{\"tail\":[2],\"feats\":[2,-0.6],\"rule\":5},{\"tail\":[1],\"feats\":[2,-0.8],\"rule\":6},{\"tail\":[0],\"feats\":[2,-0.01],\"rule\":7},{\"tail\":[2],\"feats\":[2,-0.8],\"rule\":8}],\"node\":{\"in_edges\":[4,5,6,7]}}",
  "{\"rules\":[1,\"[X] ||| [1] a\",2,\"[X] ||| [1] A\",3,\"[X] ||| [1] b\",4,\"[X] ||| [1] B'\"],\"features\":[\"f1\",\"f2\",\"Feature_1\",\"Feature_0\",\"Model_0\",\"Model_1\",\"Model_2\",\"Model_3\",\"Model_4\",\"Model_5\",\"Model_6\",\"Model_7\"],\"edges\":[],\"node\":{\"in_edges\":[]},\"edges\":[{\"tail\":[0],\"feats\":[0,-0.2],\"rule\":1},{\"tail\":[0],\"feats\":[0,-0.6],\"rule\":2}],\"node\":{\"in_edges\":[0,1]},\"edges\":[{\"tail\":[1],\"feats\":[0,-0.1],\"rule\":3},{\"tail\":[1],\"feats\":[0,-0.9],\"rule\":4}],\"node\":{\"in_edges\":[2,3]}}",
};

}

void AddNullEdge(Hypergraph* hg) {
  TRule x;
  x.arity_ = 0;
  hg->nodes_[0].in_edges_.push_back(hg->AddEdge(TRulePtr(new TRule(x)), Hypergraph::TailNodeVector())->id_);
  hg->edges_.back().head_node_ = 0;
}

void HGSetup::CreateTinyLatticeHG(Hypergraph* hg) {
    Json(hg,HGjsons[TinyLatticeHG]);
  AddNullEdge(hg);
}

void HGSetup::CreateLatticeHG(Hypergraph* hg) {
  Json(hg,HGjsons[LatticeHG]);
  AddNullEdge(hg);
}

void HGSetup::CreateHG_tiny(Hypergraph* hg) {
  Json(hg,HGjsons[HG_tiny]);
}

void HGSetup::CreateHG_int(Hypergraph* hg) {
  Json(hg,HGjsons[HG_int]);
}

void HGSetup::CreateHG(Hypergraph* hg) {
  Json(hg,HGjsons[HG]);
}

void HGSetup::CreateHGBalanced(Hypergraph* hg) {
  Json(hg,HGjsons[HGBalanced]);
}


#endif
