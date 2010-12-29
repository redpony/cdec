#include <vector>
#include <sstream>
#include <fstream>
#include <string>
#include <iostream>
#include <map>
#include "ff_dwarf.h"
#include "dwarf.h"
#include "wordid.h"
#include "tdict.h"
#include "filelib.h"
#include "sentence_metadata.h"
#include "stringlib.h"

using namespace std;

Dwarf::Dwarf(const std::string& param) { 
/* Param is a space separated string which contains any or all of the following:
   oris|orit|doms|domt=filename 
   e.g. oris=/fs/clip-galep3eval/hendra/z2e/oris128.gz
*/
  sSOS="<s>";
  sEOS="</s>";
  kSOS=TD::Convert(sSOS);
  kEOS=TD::Convert(sEOS);
  kGOAL=TD::Convert("S")*-1;
  _sent_id = (int *)malloc(sizeof(int));
  *_sent_id = -1;
  if (DEBUG) cerr << "here = " << *_sent_id << endl;
  _fwcount = (int *)malloc(sizeof(int));
  *_fwcount = -1;
  cerr << "initializing dwarf" << endl;
  flag_oris=false; flag_orit=false; flag_doms=false; flag_domt=false; flag_tfw_count=false;
  flag_bdoms=false; flag_porislr=false, flag_porisrl=false, flag_goris=false; flag_pgorislr=false, flag_pgorisrl=false;
  flag_pdomslr=false; flag_pdomsrl=false; flag_pgdomslr=false; flag_pgdomsrl=false; flag_gdoms=false;
  flag_oris_backward=false; flag_orit_backward=false; 
  explicit_soseos=false;
  SetStateSize(STATE_SIZE*sizeof(int));   
  als = new Alignment();  
  als->clearAls(Alignment::MAX_WORDS,Alignment::MAX_WORDS);
  istringstream iss(param); string w;
  while(iss >> w) {
    int equal = w.find_first_of("=");
    if (equal!=string::npos) {
      string model = w.substr(0,equal);
      vector<string> params; 
      Tokenize(w.substr(equal+1),',',&params);
      string fn = params[0];
      if (model == "minfreq") {
        cerr << "model minfreq " << fn << endl;
        als->setFreqCutoff(atoi(fn.c_str()));
      } else if (model == "oris") {
        flag_oris = readOrientation(&toris,fn,&sfw); 
        if (flag_oris) {
          oris_ = FD::Convert("OrientationSource");
          //oris_bo1_ = FD::Convert("OrientationSource_BO1");
          //oris_bo2_ = FD::Convert("OrientationSource_BO2");
        }
        if (params.size()>1) als->setAlphaOris(atof(params[1].c_str()));
        if (params.size()>2) als->setBetaOris(atof(params[2].c_str()));
      } else if (model == "porislr") {
        flag_porislr = readOrientation(&tporislr,fn,&sfw,true);
        poris_nlr = 0;
        if (flag_porislr) {
          porislr_ = FD::Convert("OrientationSourcePositionfulLeftRight");
        }
        if (params.size()>1) poris_nlr = atoi(params[1].c_str());
        if (DEBUG) cerr << "  maximum poris depth=" << poris_nlr  << endl;
      } else if (model == "porisrl") {
        flag_porisrl = readOrientation(&tporisrl,fn,&sfw,true);
        poris_nrl = 0;
        if (flag_porisrl) {
          porisrl_ = FD::Convert("OrientationSourcePositionfulRightLeft");
        }
        if (params.size()>1) poris_nrl = atoi(params[1].c_str());
        if (DEBUG) cerr << "  maximum poris depth=" << poris_nrl  << endl;
      } else if (model=="goris") {
        flag_goris = readOrientation(&tgoris,fn,&sfw);
        if (flag_goris) {
          goris_ = FD::Convert("OrientationSourceGeneralized");
        }
        if (params.size()>1) {
          readTags(params[1],&tags);
          generalizeOrientation(&tgoris,tags);          
        }
      } else if (model=="pgorislr") {
        flag_pgorislr = readOrientation(&tpgorislr,fn,&sfw,true);
        pgoris_nlr = 0;
        if (flag_pgorislr) {
          pgorislr_ = FD::Convert("OrientationSourceGeneralizedPositionfulLeftRight");
        }
        if (DEBUG) {
          cerr << "BEFORE GENERALIZATION" << endl;
          tpgorislr.print();
        }
        if (params.size()>1) pgoris_nlr = atoi(params[1].c_str());
        if (params.size()>2) {
          readTags(params[2],&tags);
          generalizeOrientation(&tpgorislr,tags,true);
        }
        if (DEBUG) {
          cerr << "AFTER GENERALIZATION" << endl;
          tpgorislr.print();
        }
      } else if (model=="pgorisrl") {
        flag_pgorisrl = readOrientation(&tpgorisrl,fn,&sfw,true);
        pgoris_nrl = 0;
        if (flag_pgorisrl) {
          pgorisrl_ = FD::Convert("OrientationSourceGeneralizedPositionfulLeftRight");
        } 
        if (params.size()>1) pgoris_nrl = atoi(params[1].c_str());
        if (params.size()>2) {
          readTags(params[2],&tags);
          generalizeOrientation(&tpgorisrl,tags,true);
        }
      } else if (model == "oris_backward") {
        flag_oris_backward = true;
        if (!flag_oris) readOrientation(&toris,fn,&sfw);
        oris_backward_ = FD::Convert("OrientationSourceBackward");
        if (params.size()>1) als->setAlphaOris(atof(params[1].c_str()));
        if (params.size()>2) als->setBetaOris(atof(params[2].c_str()));
      } else if (model == "orit") {
        flag_orit = readOrientation(&torit,fn,&tfw); 
        if (flag_orit) {
          orit_ = FD::Convert("OrientationTarget");
          //orit_bo1_ = FD::Convert("OrientationTarget_BO1");
          //orit_bo2_ = FD::Convert("OrientationTarget_BO2");
        }
        if (params.size()>1) als->setAlphaOrit(atof(params[1].c_str()));
        if (params.size()>2) als->setBetaOrit(atof(params[2].c_str()));
      } else if (model == "orit_backward") {
        flag_orit_backward = true;
        if (!flag_orit) readOrientation(&torit,fn,&tfw);
        orit_backward_ = FD::Convert("OrientationTargetBackward");
        if (params.size()>1) als->setAlphaOrit(atof(params[1].c_str()));
        if (params.size()>2) als->setBetaOrit(atof(params[2].c_str()));
      } else if (model == "doms") {
        flag_doms = readDominance(&tdoms,fn,&sfw); 
        if (flag_doms) {
          doms_ = FD::Convert("DominanceSource");
          //doms_bo1_ = FD::Convert("DominanceSource_BO1");
          //doms_bo2_ = FD::Convert("DominanceSource_BO2");
        }
        if (params.size()>1) als->setAlphaDoms(atof(params[1].c_str()));
        if (params.size()>2) als->setBetaDoms(atof(params[2].c_str()));
      } else if (model == "pdomsrl") {
        flag_pdomsrl = readDominance(&tpdomsrl,fn,&sfw,true);
        if (flag_pdomsrl) {
          pdomsrl_ = FD::Convert("DominanceSourcePositionfulRightLeft");
        }
        if (params.size()>1) pdoms_nrl = atoi(params[1].c_str());
      } else if (model == "pdomslr") {
        flag_pdomslr = readDominance(&tpdomslr,fn,&sfw,true);
        tpdomslr.print();
        if (flag_pdomslr) {
          pdomslr_ = FD::Convert("DominanceSourcePositionfulLeftRight");
        }
        if (params.size()>1) pdoms_nlr = atoi(params[1].c_str());
      } else if (model == "pgdomsrl") {
        flag_pgdomsrl = readDominance(&tpgdomsrl,fn,&sfw,true);
        if (flag_pgdomsrl) {
          pgdomsrl_ = FD::Convert("DominanceSourceGeneralizedPositionfulRightLeft");
        }
        if (params.size()>1) pgdoms_nrl = atoi(params[1].c_str());
        if (params.size()>2) {
          readTags(params[2],&tags);
          generalizeDominance(&tpgdomsrl,tags,true);
        }
      } else if (model == "pgdomslr") {
        flag_pgdomslr = readDominance(&tpgdomslr,fn,&sfw,true);
        if (flag_pgdomslr) {
          pgdomslr_ = FD::Convert("DominanceSourceGeneralizedPositionfulLeftRight");
        }
        if (params.size()>1) pgdoms_nlr = atoi(params[1].c_str());
        if (params.size()>2) {
          readTags(params[2],&tags);
          if (DEBUG) {
            for (map<WordID,WordID>::const_iterator it=tags.begin(); it!=tags.end(); it++) {
              cerr << "tags = " << TD::Convert(it->first) << ", " << TD::Convert(it->second) << endl;
            }
          } 
          generalizeDominance(&tpgdomslr,tags,true);
        }
        if (DEBUG) tpgdomslr.print();
      } else if (model == "bdoms") {
        flag_bdoms = readDominance(&tbdoms,fn,&sfw);
        if (flag_bdoms) {
          bdoms_ = FD::Convert("BorderDominanceSource");
        }
      } else if (model == "domt") {
        flag_domt = readDominance(&tdomt,fn,&tfw); 
        if (flag_domt) {
          domt_ = FD::Convert("DominanceTarget");
          //domt_bo1_ = FD::Convert("DominanceTarget_BO1");
          //domt_bo2_ = FD::Convert("DominanceTarget_BO2");
        }
        if (params.size()>1) als->setAlphaDomt(atof(params[1].c_str()));
        if (params.size()>2) als->setBetaDomt(atof(params[2].c_str()));
      } else if (model== "tfw_count") {
        flag_tfw_count = readList(fn,&tfw);
        tfw_count_ = FD::Convert("TargetFunctionWordsCount");        
      } else {
        cerr << "DWARF doesn't understand this model: " << model << endl;
      }
    } else {
      if (w=="tfw_count") {
        flag_tfw_count = true;
        tfw_count_ = FD::Convert("TargetFunctionWordsCount");
      } else if (w=="oris_backward") {
        flag_oris_backward = true;
        oris_backward_ = FD::Convert("OrientationSourceBackward"); 
      } else if (w=="orit_backward") {
        flag_orit_backward = true;
        orit_backward_ = FD::Convert("OrientationTargetBackward");
      } else if (w=="explicit_soseos") {
        explicit_soseos=true;
      } else {
        cerr << "DWARF doesn't need this param: " << param << endl; 
      }
    }  
  }
  for (map<WordID,int>::const_iterator it=sfw.begin(); it!=sfw.end() && DEBUG; it++) {
    cerr << "   FW:" << TD::Convert(it->first) << endl;
  }
}

void Dwarf::TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const {
  if (DEBUG) cerr << "TraversalFeaturesImpl" << endl;
  double cost, bonus, bo1, bo2, bo1_bonus, bo2_bonus;
  double bdoms_state_mono= 0; double bdoms_state_nonmono = 0;
  TRule r = *edge.rule_;
  if (DEBUG) cerr << " sent_id=" << *_sent_id << ", " << smeta.GetSentenceID() << endl;
  if (DEBUG) cerr << "rule = " << r.AsString() << endl; 
  if (DEBUG) cerr << "rule[i,j] = " << edge.i_ << "," << edge.j_ << endl;
  if (*_sent_id != smeta.GetSentenceID()) { //new sentence
    *_sent_id = smeta.GetSentenceID();
    const Lattice l = smeta.GetSourceLattice();
    *_fwcount=0;
    for (int i=0; i<smeta.GetSourceLength(); i++) {
      if (sfw.find(l[i][0].label)!=sfw.end()) {
        *_fwcount+=1;
      }
    }
    if (DEBUG) cerr << "new sentence[" << *_sent_id << "]="<<*_fwcount<<endl;
  }
  bool nofw = als->prepare(*edge.rule_, ant_contexts, sfw, tfw,smeta.GetSourceLattice(),edge.i_,edge.j_); 
  bool isFinal = (edge.i_==0 && edge.j_==smeta.GetSourceLength() && r.GetLHS()==kGOAL);
  // prepare *nofw* outputs whether the resulting alignment, contains function words or not
  // if not, the models do not have to be calcualted and *simplify* is very simple
  if (DEBUG) cerr << "nofw = " << nofw << endl;
  if (flag_tfw_count) {
    double count = 0;
    for (int i=0; i<r.e_.size(); i++) {
      if (tfw.find(r.e_[i])!=tfw.end()) count++;
    }
    features->set_value(tfw_count_,count);  
  }
  if (flag_oris) {
    cost=0; bonus=0; bo1=0; bo2=0; bo1_bonus=0; bo2_bonus=0;
    if (!nofw) als->computeOrientationSource(toris,&cost,&bonus,&bo1,&bo1_bonus,&bo2,&bo2_bonus); 
    if (isFinal&&!explicit_soseos) {
      cost += bonus;
      bonus = 0;
    }
    features->set_value(oris_,cost); 
    //features->set_value(oris_bo1_,bo1); 
    //features->set_value(oris_bo2_,bo2);
    estimated_features->set_value(oris_,bonus); 
    //estimated_features->set_value(oris_bo1_,bo1_bonus); 
    //estimated_features->set_value(oris_bo2_,bo2_bonus);
  }
  if (flag_porislr) {
    cost=0; bonus=0; bo1=0; bo2=0; bo1_bonus=0; bo2_bonus=0;
    if (!nofw) 
      als->computeOrientationSourcePos(tporislr,&cost,&bonus,&bo1,&bo1_bonus,&bo2,&bo2_bonus,*_fwcount,poris_nlr,0);
    if (isFinal&&!explicit_soseos) {
      cost += bonus;
      bonus = 0;
    }
    features->set_value(porislr_,cost);
    estimated_features->set_value(porislr_,bonus);
  }
  if (flag_porisrl) {
    cost=0; bonus=0; bo1=0; bo2=0; bo1_bonus=0; bo2_bonus=0;
    if (!nofw)
      als->computeOrientationSourcePos(tporisrl,&cost,&bonus,&bo1,&bo1_bonus,&bo2,&bo2_bonus,*_fwcount,0,poris_nrl);
    if (isFinal&&!explicit_soseos) {
      cost += bonus;
      bonus = 0;
    }
    features->set_value(porisrl_,cost);
    estimated_features->set_value(porisrl_,bonus);
  }
  if (flag_pgorislr) {
    cost=0; bonus=0; bo1=0; bo2=0; bo1_bonus=0; bo2_bonus=0;
    if (!nofw)
      als->computeOrientationSourcePos(tpgorislr,&cost,&bonus,&bo1,&bo1_bonus,&bo2,&bo2_bonus,*_fwcount,pgoris_nlr,0);
    if (isFinal&&!explicit_soseos) {
      cost += bonus;
      bonus = 0;
    }
    features->set_value(pgorislr_,cost);
    estimated_features->set_value(pgorislr_,bonus);
  }
  if (flag_pgorisrl) {
    cost=0; bonus=0; bo1=0; bo2=0; bo1_bonus=0; bo2_bonus=0;
    if (!nofw)
      als->computeOrientationSourcePos(tpgorisrl,&cost,&bonus,&bo1,&bo1_bonus,&bo2,&bo2_bonus,*_fwcount,0,pgoris_nrl);
    if (isFinal&&!explicit_soseos) {
      cost += bonus;
      bonus = 0;
    }
    features->set_value(pgorisrl_,cost);
    estimated_features->set_value(pgorisrl_,bonus);
  }
  if (flag_goris) {
    cost=0; bonus=0;
    if (!nofw) als->computeOrientationSource(tgoris,&cost,&bonus,&bo1,&bo1_bonus,&bo2,&bo2_bonus);
    if (isFinal&&!explicit_soseos) {
      cost += bonus;
      bonus = 0;
    }
    features->set_value(goris_,cost);
    estimated_features->set_value(goris_,bonus);
  }
  if (flag_oris_backward) {
    cost=0; bonus=0;
    if (!nofw) 
      als->computeOrientationSourceBackward(toris,&cost,&bonus,&bo1,&bo1_bonus,&bo2,&bo2_bonus);
    if (isFinal&&!explicit_soseos) {
      cost += bonus;
      bonus = 0;
    }
    features->set_value(oris_backward_,cost);
    estimated_features->set_value(oris_backward_,bonus);
  }
  WordID _lfw = kSOS;
  WordID _rfw = kEOS; 
  if (flag_doms || flag_pdomslr || flag_pdomsrl || flag_pgdomslr || flag_pgdomsrl) {
    if (DEBUG) cerr << "   seeking lfw and rfw" << endl;
    int start = edge.i_;
    int end   = edge.j_;
    if (DEBUG) cerr << "   start=" << start << ", end=" << end << endl;
    const Lattice l = smeta.GetSourceLattice();
    for (int idx=start-1; idx>=0; idx--) {
      if (DEBUG) cerr << "  checking idx=" << idx << ", label=" << l[idx][0].label << "-" << TD::Convert(l[idx][0].label) << endl;
      if (sfw.find(l[idx][0].label) !=sfw.end()) {
        if (DEBUG) cerr << "+";
        _lfw=l[idx][0].label; break;
      }
    }
    for (int idx=end; idx<l.size(); idx++) { // end or end+1
      if (DEBUG) cerr << "  checking idx=" << idx << ", label=" << l[idx][0].label << "-" << TD::Convert(l[idx][0].label) << endl;
      if (sfw.find(l[idx][0].label)!=sfw.end()) {
        if (DEBUG) cerr << ".";
        _rfw=l[idx][0].label; break;
      }
    }
    if (isFinal&&!explicit_soseos) {
      _lfw=kSOS; _rfw=kEOS;
    }
  }
  if (flag_doms) {
    cost=0; bonus=0; bo1=0; bo2=0; bo1_bonus=0; bo2_bonus=0;
    if (!nofw) als->computeDominanceSource(tdoms,_lfw,_rfw,&cost,&bonus,
                                           &bo1,&bo1_bonus,&bo2,&bo2_bonus); 
    if (DEBUG) cerr << "   COST=" << cost << ", BONUS=" << bonus << endl;
    if (isFinal&&!explicit_soseos) {
      cost += bonus;
      if (DEBUG) cerr << "    final and !explicit_soseos, thus cost = " << cost <<  endl;
      bonus = 0;
    }
    features->set_value(doms_,cost); 
    estimated_features->set_value(doms_,bonus);
  }
  if (flag_pdomslr) {
   if (DEBUG) cerr << " flag_pdomslr true, nofw=" << nofw << endl;
   if (DEBUG) cerr << "   lfw=" << _lfw << ", rfw=" << _rfw << endl;
   if (DEBUG) cerr << "   kSOS=" << kSOS << ", kEOS=" << kEOS << endl;
    cost=0; bonus=0; bo1=0; bo2=0; bo1_bonus=0; bo2_bonus=0;
    if (!nofw) als->computeDominanceSourcePos(tpdomslr,_lfw,_rfw,&cost,&bonus,
                                           &bo1,&bo1_bonus,&bo2,&bo2_bonus,*_fwcount,pdoms_nlr,0);
    if (isFinal&&!explicit_soseos) {
      cost += bonus;
      bonus = 0;
    }
    features->set_value(pdomslr_,cost);
    estimated_features->set_value(pdomslr_,bonus);  
  }
  if (flag_pdomsrl) {
    cost=0; bonus=0; bo1=0; bo2=0; bo1_bonus=0; bo2_bonus=0;
    if (!nofw) als->computeDominanceSourcePos(tpdomsrl,_lfw,_rfw,&cost,&bonus,
                                           &bo1,&bo1_bonus,&bo2,&bo2_bonus,*_fwcount,0,pdoms_nrl);
    if (isFinal&&!explicit_soseos) {
      cost += bonus;
      bonus = 0;
    }
    features->set_value(pdomsrl_,cost);
    estimated_features->set_value(pdomsrl_,bonus); 
  }
  if (flag_pgdomslr) {
    cost=0; bonus=0; bo1=0; bo2=0; bo1_bonus=0; bo2_bonus=0;
    if (!nofw) als->computeDominanceSourcePos(tpgdomslr,_lfw,_rfw,&cost,&bonus,
                                           &bo1,&bo1_bonus,&bo2,&bo2_bonus,*_fwcount,pgdoms_nlr,0);
    if (isFinal&&!explicit_soseos) {
      cost += bonus;
      bonus = 0;
    }
    features->set_value(pgdomslr_,cost);
    estimated_features->set_value(pgdomslr_,bonus);  
  }
  if (flag_pgdomsrl) {    cost=0; bonus=0; bo1=0; bo2=0; bo1_bonus=0; bo2_bonus=0;
    if (!nofw) als->computeDominanceSourcePos(tpgdomsrl,_lfw,_rfw,&cost,&bonus,
                                           &bo1,&bo1_bonus,&bo2,&bo2_bonus,*_fwcount,0,pgdoms_nrl);
    if (isFinal&&!explicit_soseos) {
      cost += bonus;
      bonus = 0;
    }
    features->set_value(pgdomsrl_,cost);
    estimated_features->set_value(pgdomsrl_,bonus); 
  }


  if (flag_bdoms) {
    cost=0; bonus=0; bdoms_state_mono=0; bdoms_state_nonmono=0; 
    if (!nofw)
      als->computeBorderDominanceSource(tbdoms,&cost,&bonus,
        &bdoms_state_mono, &bdoms_state_nonmono,*edge.rule_, ant_contexts, sfw);
    features->set_value(bdoms_,cost);
    estimated_features->set_value(bdoms_,bonus); 
  }
  if (flag_orit) {
    cost=0; bonus=0; bo1=0; bo2=0; bo1_bonus=0; bo2_bonus=0;
    if (!nofw) als->computeOrientationTarget(torit,&cost,&bonus,&bo1,&bo1_bonus,&bo2,&bo2_bonus); 
    if (DEBUG) cerr << "cost=" << cost << ", bonus=" << bonus << ", bo1=" << bo1 << ", bo1_bonus=" << bo1_bonus << ", bo2=" << bo2 << ", bo2_bonus=" << bo2_bonus << endl;
    features->set_value(orit_,cost); 
    //features->set_value(orit_bo1_,bo1); 
    //features->set_value(orit_bo2_,bo2);
    estimated_features->set_value(orit_,bonus);
    //estimated_features->set_value(orit_bo1_,bo1_bonus);
    //estimated_features->set_value(orit_bo2_,bo2_bonus);
  }
  if (flag_orit_backward) {
    cost=0; bonus=0;
    if (!nofw) als->computeOrientationTargetBackward(torit,&cost,&bonus,&bo1,&bo1_bonus,&bo2,&bo2_bonus);
    features->set_value(orit_backward_,cost);
    estimated_features->set_value(orit_backward_,bonus);
  }
  if (flag_domt) {
    cost=0; bonus=0; bo1=0; bo2=0; bo1_bonus=0; bo2_bonus=0;
    WordID _lfw=-1; int start = edge.i_;
    WordID _rfw=-1; int end   = edge.j_;
    if (smeta.HasReference()) {
      const Lattice l = smeta.GetReference();
      for (int idx=start-1; idx>=0; idx--) {
        if (l.size()>0)
          if (tfw.find(l[idx][0].label) !=tfw.end()) {
            _lfw=l[idx][0].label; break;
          }
      }
      for (int idx=end; idx<l.size(); idx++) { // end or end+1
        if (l[idx].size()>0)
          if (tfw.find(l[idx][0].label)!=tfw.end()) {
            _rfw=l[idx][0].label; break;
          }
      }
    }
    //neighboringFWs(smeta.GetReference(),edge.i_,edge.j_,tfw,&_lfw,&_rfw);
    if (!nofw) als->computeDominanceTarget(tdomt,_lfw,_rfw,&cost,&bonus,
                                           &bo1,&bo1_bonus,&bo2,&bo2_bonus);
    features->set_value(domt_,cost); 
    //features->set_value(domt_bo1_,bo1); 
    //features->set_value(domt_bo2_,bo2);
    estimated_features->set_value(domt_,bonus);
    //estimated_features->set_value(domt_bo1_,bo1_bonus);
    //estimated_features->set_value(domt_bo2_,bo2_bonus);
  }
  int* vcontext = reinterpret_cast<int *>(context);
  if (!nofw) {
    als->BorderingSFWsOnly();
    als->BorderingTFWsOnly();
    als->simplify(vcontext);  
  } else {
    als->simplify_nofw(vcontext);
  }
  vcontext[50] = DoubleToInteger(bdoms_state_mono);
  vcontext[51] = DoubleToInteger(bdoms_state_nonmono);
  vcontext[STATE_SIZE-1] = Alignment::link(edge.i_,edge.j_); 
  if (DEBUG) {
    cerr << "state@traverse = ";
    for (int idx=0; idx<STATE_SIZE; idx++) cerr << idx << "." << vcontext[idx] << " ";
    cerr << endl;
    cerr << "bdoms_state_mono=" << bdoms_state_mono << ", state[50]=" << IntegerToDouble(vcontext[50]) << endl;
    cerr << "bdoms_state_nonmono=" << bdoms_state_nonmono << ", state[51]=" << IntegerToDouble(vcontext[51]) << endl;
  }
}

int Dwarf::DoubleToInteger(double val) {
  float x = (float)val;
  float* px = &x;
  int* pix = reinterpret_cast<int *>(px);
  return *pix; 
}

double Dwarf::IntegerToDouble(int val) {
  int *py = &val;
  float* pd = reinterpret_cast<float *>(py);
  return (double)*pd;
}

void Dwarf::neighboringFWs(const Lattice& l, const int& i, const int& j, const map<WordID,int>& fw_hash, int* lfw, int* rfw) {
  *lfw=0; *rfw=0;
  int idx=i-l[i][0].dist2next;
  while (idx>=0) {
    if (l[idx].size()>0) { 
      if (fw_hash.find(l[idx][0].label)!=fw_hash.end()) {
        *lfw++;  
      }
    }
    idx-=l[idx][0].dist2next;
  }
  idx=j+l[j][0].dist2next;
  while (idx<l.size()) {
    if (l[idx].size()>0) { 
      if (fw_hash.find(l[idx][0].label)!=fw_hash.end()) {
        *rfw++;
      }
    }
    idx+=l[idx][0].dist2next;
  }
}

bool Dwarf::readOrientation(CountTable* table, const std::string& filename, std::map<WordID,int> *fw, bool pos) {
  // the input format is
  // source target 0 1 2 3 4 0 1 2 3 4
  // 0 -> MA, 1 -> RA, 2 -> MG, 3 -> RG, 4 -> NO_NEIGHBOR
  // first 01234 corresponds to the left neighbor, the second 01234 corresponds to the right neighbor
  // append 2 more at the end as precomputed total
  
  // TONS of hack here. CountTable should be wrapped as a class  
  // TODO: check whether the file exists or not, return false if not
  if (DEBUG) cerr << "  readOrientation(" << filename << ", pos=" << pos << ")" << endl;
  ReadFile rf(filename);  
  istream& in = *rf.stream();
  table->setup(24,pos);
  table->ultimate = new int[24];
  for (int i=0; i<24; i++) table->ultimate[i]=0;
  ostringstream oss;
  while (in) {
    string line;
    getline(in,line);
    if (line=="") break;
    istringstream tokenizer(line);
    string sourceidx, source, target, word;
    tokenizer >> source >> target; 
    if (pos) {
      sourceidx = source;
      source = sourceidx.substr(0,sourceidx.find_last_of("/"));
    }
    if (fw->find(TD::Convert(source))==fw->end()) fw->insert(pair<WordID,int>(TD::Convert(source),1));


    int* element = new int[24];
    element[5] = 0;
    for (int i=0; i<5; i++) {
      element[i] = 0;
      if (tokenizer >> word) element[i] = atoi(word.c_str());
      element[5] += element[i];
    }
    element[11] = 0;
    for (int i=6; i<11; i++) {
      element[i] = 0;
      if (tokenizer >> word) element[i] = atoi(word.c_str());
      element[11] += element[i];
    }
    element[17] = 0;
    for (int i=12; i<17; i++) {
      element[i] = 0;
      if (tokenizer >> word) element[i] = atoi(word.c_str());
      element[17] += element[i];
    }
    element[23] = 0;
    for (int i=18; i<23; i++) {
      element[i] = 0;
      if (tokenizer >> word) element[i] = atoi(word.c_str());
      element[23] += element[i];
    }
    for (int i=0; i<24; i++) table->ultimate[i] += element[i];
    oss << source << " " << target;
    WordID key_id = TD::Convert(oss.str());
    oss.str("");
    if (table->model.find(key_id)!=table->model.end()) {  
      for (int i=0; i<24; i++) table->model[key_id][i]+=element[i];
    } else {
      int* el2 = new int[24];
      for (int i=0; i<24; i++) el2[i] = element[i];
      table->model.insert(pair<WordID,int*>(key_id,el2));
    }
    
    oss << source;
    key_id = TD::Convert(oss.str());
    oss.str("");
    if (table->model.find(key_id)!=table->model.end()) {    
      for (int i=0; i<24; i++) table->model[key_id][i]+=element[i];
    } else {
      int* el2 = new int[24];
      for (int i=0; i<24; i++) el2[i] = element[i];
      table->model.insert(pair<WordID,int*>(key_id,el2));
    }

    if (pos) {
      oss << sourceidx << " " << target;
      key_id = TD::Convert(oss.str());
      oss.str(""); 
      if (table->model.find(key_id)!=table->model.end()) {
        for (int i=0; i<24; i++) table->model[key_id][i]+=element[i];
      } else {
        int* el2 = new int[24];
        for (int i=0; i<24; i++) el2[i] = element[i];
        table->model.insert(pair<WordID,int*>(key_id,el2));
      }
    }
    delete[] element;
  }  
  return true;    
}

bool Dwarf::readList(const std::string& filename, std::map<WordID,int>* fw) {
  ReadFile rf(filename);
  istream& in = *rf.stream();
  while (in) {
    string word;
    getline(in,word);
    if (fw->find(TD::Convert(word))==fw->end()) fw->insert(pair<WordID,int>(TD::Convert(word),1)); 
  }
  return true;
}

bool Dwarf::readDominance(CountTable* table, const std::string& filename, std::map<WordID,int>* fw, bool pos) {
  // the input format is 
  // source1 source2 target1 target2 0 1 2 3
  // 0 -> dontcase 1->leftfirst 2->rightfirst 3->neither 
  if (DEBUG) cerr << "readDominance(" << filename << ",pos="<< pos << ")" << endl;
  ReadFile rf(filename);
  istream& in = *rf.stream();
  table->ultimate = new int[5];
  table->setup(5,pos);
  for (int i=0; i<5; i++) table->ultimate[i]=0;
  while (in) {
    string line, word;
    getline(in,line);
    if (line=="") break;
    string source1idx, source2idx, target1, target2, source1, source2;
    ostringstream oss; 
    WordID key_id;
    istringstream tokenizer(line);
    tokenizer >> source1 >> source2 >> target1 >> target2; 
    if (pos) {
      source1idx = source1;
      source2idx = source2;
      source1 = source1idx.substr(0,source1idx.find_last_of("/"));
      source2 = source2idx.substr(0,source2idx.find_last_of("/"));
    }
    if (fw->find(TD::Convert(source1))==fw->end()) fw->insert(pair<WordID,int>(TD::Convert(source1),1));
    if (fw->find(TD::Convert(source2))==fw->end()) fw->insert(pair<WordID,int>(TD::Convert(source2),1));

    int* element = new int[5];
    element[4]=0;
    for (int i=0; i<4; i++) {
      element[i]  = 0;
      if (tokenizer >> word) element[i] = atoi(word.c_str());
      element[4]+=element[i];
    }
    for (int i=0; i<5; i++) table->ultimate[i] += element[i];

    oss << source1 << " " << source2 << " " << target1 << " " << target2;
    key_id = TD::Convert(oss.str());
    oss.str("");
    if (table->model.find(key_id)!=table->model.end()) { 
      for (int i=0; i<5; i++) table->model[key_id][i]+=element[i];
    } else {
      int* el2 = new int[5]; 
      for (int i=0; i<5; i++) el2[i]=element[i];
      table->model.insert(pair<WordID,int*>(key_id,el2));
    }

    oss << source1 << " " << source2;
    key_id = TD::Convert(oss.str());
    oss.str("");
    if (table->model.find(key_id)!=table->model.end()) {  
      for (int i=0; i<5; i++) table->model[key_id][i]+=element[i];
    } else {
      int* el2 = new int[5]; 
      for (int i=0; i<5; i++) el2[i]=element[i];
      table->model.insert(pair<WordID,int*>(key_id,el2));
    }

    if (pos) {
      oss << source1idx << " " << source2idx << " " << target1 << " " << target2;
      key_id = TD::Convert(oss.str());
      oss.str("");
      if (table->model.find(key_id)!=table->model.end()) {  
        for (int i=0; i<5; i++) table->model[key_id][i]+=element[i];
      } else {
        int* el2 = new int[5]; 
        for (int i=0; i<5; i++) el2[i]=element[i];
        table->model.insert(pair<WordID,int*>(key_id,el2));
      }
    }
    delete element;
  }

  return true;    
}

bool Dwarf::readTags(const std::string& filename, std::map<WordID,WordID>* tags) {
  ReadFile rf(filename);
  istream& in = *rf.stream();
  while(in) {
    string line, word, tag;
    getline(in,line);
    if (line=="") break;
    istringstream tokenizer(line);
    tokenizer >> tag >> word;
    tags->insert(pair<WordID,WordID>(TD::Convert(word),TD::Convert(tag)));
  }
  return true;
}

bool Dwarf::generalizeOrientation(CountTable* table, const std::map<WordID,WordID>& tags, bool pos) {
  map<string,int*> generalized;
  for (map<WordID,int*>::iterator it=table->model.begin(); it!=table->model.end(); it++) {
    string source, target;
    istringstream tokenizer(TD::Convert(it->first));
    tokenizer >> source >> target;
    string idx = "";
    if (pos) {
      int found = source.find_last_of("/");
      if (found!=string::npos && found>0) { 
        idx = source.substr(found+1);
        source = source.substr(0,found);
      }
    }
    map<WordID,WordID>::const_iterator tags_iter = tags.find(TD::Convert(source));
    if (tags_iter!=tags.end()) {
      ostringstream genkey;
      genkey << TD::Convert(tags_iter->second);
      if (idx!="") genkey << "/" << idx;
      if (target!="") genkey << " " << target;
      int* model;
      if (generalized.find(genkey.str())!=generalized.end()) {
        model = generalized[genkey.str()];
        for (int i=0; i<24; i++) model[i] += it->second[i];
      } else {
        int* el = new int[24];
        for (int i=0; i<24; i++) el[i] = it->second[i];
        generalized.insert(pair<string,int*>(genkey.str(),el));
      }
    }
  }
  for (map<WordID,int*>::iterator it=table->model.begin(); it!=table->model.end(); it++) {
    string source, target;
    istringstream tokenizer(TD::Convert(it->first));
    tokenizer >> source >> target;
    string idx = "";
    if (pos) {
      int found = source.find_last_of("/");
      if (found!=string::npos && found>0) {
        idx = source.substr(found+1);
        source = source.substr(0,found);
      }
    }
    map<WordID,WordID>::const_iterator tags_iter = tags.find(TD::Convert(source));
    if (tags_iter!=tags.end()) {
      ostringstream genkey;
      genkey << TD::Convert(tags_iter->second);
      if (idx!="") genkey << "/" << idx;
      if (target!="") genkey << " " << target;
      if (generalized.find(genkey.str())!=generalized.end()) {
        delete it->second;
        it->second = generalized[genkey.str()];
      }
    }
  }

}
 


bool Dwarf::generalizeDominance(CountTable* table, const std::map<WordID,WordID>& tags, bool pos) {
  map<string,int*> generalized;
  ostringstream oss;
  for (map<WordID,int*>::iterator it=table->model.begin(); it!=table->model.end(); it++) {
    string source1, source2, target1, target2;
    string idx1 = ""; string idx2 = "";
    istringstream tokenizer(TD::Convert(it->first));
    tokenizer >> source1 >> source2 >> target1 >> target2;
    if (DEBUG) cerr << "source1=|" << source1 << "|, source2=|" << source2 << "|, target1=|" << target1 << "|, target2=|" << target2 << "|" << endl;
    if (pos) {
      int found1 = source1.find_last_of("/");
      int found2 = source2.find_last_of("/");
      if (found1!=string::npos && found2!=string::npos && found1>0 && found2>0) {
        idx1 = source1.substr(found1+1);
        source1 = source1.substr(0,found1);
        idx2 = source2.substr(found2+1);
        source2 = source2.substr(0,found2);
      }
    }
    if (DEBUG) 
      cerr << "[U]source1='" << source1 << "', idx1='"<< idx1 << "', source2='" << source2 << "', idx2='"<< idx2 << "', target1='" << target1 << "', target2='" << target2 << "'" << endl;
    map<WordID,WordID>::const_iterator tags_iter1 = tags.find(TD::Convert(source1));
    map<WordID,WordID>::const_iterator tags_iter2 = tags.find(TD::Convert(source2));
    if (tags_iter1!=tags.end()) 
      source1 = TD::Convert(tags_iter1->second);
    oss << source1;
    if (idx1!="") oss << "/" << idx1;
    if (tags_iter2!=tags.end())
      source2 = TD::Convert(tags_iter2->second);
    oss << " " << source2;
    if (idx2!="") oss << "/" << idx2;
    if (target1!="" && target2!="") oss << " " << target1 << " " << target2;
    
    if (DEBUG) cerr << "generalized key = '" << oss.str() << "'" << endl; 
    if (generalized.find(oss.str())!=generalized.end()) {
      int* model = generalized[oss.str()];
      for (int i=0; i<5; i++) model[i] += it->second[i];
    } else {
      int* model = new int[5];
      for (int i=0; i<5; i++) model[i] = it->second[i];
      generalized.insert(pair<string,int*>(oss.str(),model));
    }    
    oss.str("");
  }
  
  if (DEBUG) {
    for (map<string,int*>::const_iterator it=generalized.begin(); it!=generalized.end(); it++) {
      cerr << "GENERALIZED = " << it->first << ", ";
      for (int i=0; i<5; i++) cerr << it->second[i] << " ";
      cerr << endl;
    }
  }

  for (map<WordID,int*>::iterator it=table->model.begin(); it!=table->model.end(); it++) {
    string source1, source2, target1, target2;
    string idx1 = ""; string idx2 = "";
    istringstream tokenizer(TD::Convert(it->first));
    tokenizer >> source1 >> source2 >> target1 >> target2;
    if (pos) {
      int found1 = source1.find_last_of("/");
      int found2 = source2.find_last_of("/");
      if (found1!=string::npos && found2!=string::npos && found1>0 && found2>0) {
        idx1 = source1.substr(found1+1);
        source1 = source1.substr(0,found1);
        idx2 = source2.substr(found2+1);
        source2 = source2.substr(0,found2);
      }
    }
    map<WordID,WordID>::const_iterator tags_iter1 = tags.find(TD::Convert(source1));
    map<WordID,WordID>::const_iterator tags_iter2 = tags.find(TD::Convert(source2));
    if (tags_iter1!=tags.end())
      source1 = TD::Convert(tags_iter1->second);
    oss << source1;
    if (idx1!="") oss << "/" << idx1;
    if (tags_iter2!=tags.end())
      source2 = TD::Convert(tags_iter2->second);
    oss << " " << source2;
    if (idx2!="") oss << "/" << idx2;
    if (target1!="" && target2!="") oss << " " << target1 << " " << target2;
    
    if (generalized.find(oss.str())!=generalized.end()) {
      if (DEBUG) cerr << " generalizing "<< TD::Convert(it->first) << " into " << oss.str() << endl; 
      if (DEBUG) {
        cerr << "  model from ";
        for (int i=0; i<5; i++) cerr << it->second[i] << " "; 
        cerr << endl;
      }
      delete it->second;
      it->second = generalized[oss.str()];
      if (DEBUG) {
        cerr << "  into ";
        for (int i=0; i<5; i++) cerr << it->second[i] << " "; 
        cerr << endl;
      }
    }    
    oss.str("");
  }

}
