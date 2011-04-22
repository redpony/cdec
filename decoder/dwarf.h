#ifndef DWARF_H
#define DWARF_H

#include <cstdlib>
#include <vector>
#include <map>
#include <string>
#include <ostream>
#include "wordid.h"
#include "lattice.h"
#include "trule.h"
#include "tdict.h"
#include <boost/functional/hash.hpp>
#include <tr1/unordered_map>
#include <boost/tuple/tuple.hpp>

using namespace std;
using namespace std::tr1;
using namespace boost::tuples;
using namespace boost;

const static bool DEBUG = false;

class CountTable {
public:
        int* ultimate;
        map<WordID,int*> model;
        int mode;
        int numColumn;
        void print() const;
        void setup(int _numcolumn, int _mode) {
          mode = _mode; numColumn = _numcolumn;
        }
};

class Alignment {
/* Alignment represents an alignment object in a 2D format to support function word-based models calculation 

   A note about model's parameter estimation:
   ==========================================
   The model is estimated as a two-level Dirichlet process. 
   For orientation model, the first tier estimation is:
   P(o|f,e) where *o* is the orientation value to estimate, *f* is the source function word aligned to *e* 
   its second tier is: P(o|f), while its third tier is P(o)
   For dominance model, the first tier estimation is:
   P(d|f1,f2,e1,e2) where *d* is a dominance value to estimate, *f1,f2* are the neighboring function words on the source
   aligned to *e1,e2* on the target side
   its second tier is: P(d|f1,f2) while its third tier is P(d)
    
   Taking orientation model as a case in point, a two level estimation proceeds as follow:
   P(o|f,e) = c(o,f,e) + alpha { c(o,f) + beta [ c (o) / c(.) ] }
                                 ------------------------------
                                 c(f)   + beta
              -------------------------------------------------
              c(f,e)   + alpha
   where c() is a count function, alpha and beta are the concentration parameter 
         of the first and second Dirichlet process respectively 
   To encourage or penalize the use of second and third tier statistics, bo1 and bo2 binary features are introduced 
*/
public:
  const static int MAX_WORDS = 200;  
  const static int MINIMUM_INIT = 1000;
  const static int MAXIMUM_INIT = -1000;
  const static int MAX_ARITY = 2;
  WordID kSOS;
  WordID kEOS;
  WordID kUNK;
  double alpha_oris; // 1st concentration parameter for orientation model 
  double beta_oris;  // 2nd concentration parameter for orientation model
  double alpha_orit; // 1st concentration parameter for orientation model 
  double beta_orit;  // 2nd concentration parameter for orientation model
  double alpha_doms; // idem as above but for dominance model
  double beta_doms;
  double alpha_domt; // idem as above but for dominance model
  double beta_domt;
  
  // ACCESS to alignment
  void set(int j,int i);   // j is the source index, while i is the target index
  void reset(int j,int i); // idem as above
  inline bool at(int j, int i) { return _matrix[j][i]; };
  inline int getJ() {return _J;}; // max source of the current alignment
  inline int getI() {return _I;}; // max target of the current alignment
  inline void setI(int I) { _I = I; };
  inline void setJ(int J) { _J = J; };
  inline void setF(vector<WordID> f) { _f=f;};
  inline void setE(vector<WordID> e) { _e=e;};
  inline WordID getF(int id) { if (id<0) return TD::Convert("<s>"); if (id>=_f.size()) return TD::Convert("</s>"); return _f[id];};
  inline WordID getE(int id) { if (id<0) return TD::Convert("<s>"); if (id>=_e.size()) return TD::Convert("</s>"); return _e[id];};
  void clearAls(int prevJ=200, int prevI=200);
  int sourceOf(int i, int start = -1);
  int targetOf(int j, int start = -1);
  inline int minSSpan(int i) { return _sSpan[i][0];}
  inline int maxSSpan(int i) { return _sSpan[i][1];}
  inline int minTSpan(int j) { return _tSpan[j][0];}
  inline int maxTSpan(int j) { return _tSpan[j][1];}
  static inline int link(int s, int t) { return (s << 16) | t; }
  static inline int source(int st) {return st >> 16; }
  static inline int target(int st) {return st & 0xffff; }
  inline void setAlphaOris(double val) { alpha_oris=val; }
  inline void setAlphaOrit(double val) { alpha_orit=val; }
  inline void setAlphaDoms(double val) { alpha_doms=val; }
  inline void setAlphaDomt(double val) { alpha_domt=val; }
  inline void setBetaOris(double val) { beta_oris=val; }
  inline void setBetaOrit(double val) { beta_orit=val; }
  inline void setBetaDoms(double val) { beta_doms=val; }
  inline void setBetaDomt(double val) { beta_domt=val; }
  inline void setFreqCutoff(int val) { cout << _freq_cutoff << " to " << val << endl;  _freq_cutoff=val; }
  string AsString();
  string AsStringSimple();
  int* SOS();
  int* EOS();

  // Model related function  
  Alignment();
  // Given the current *rule* and its antecedents, construct an alignment space and mark the function word alignments 
  // according *sfw* and *tfw*
  bool prepare(TRule& rule, const std::vector<const void*>& ant_contexts, 
               const map<WordID,int>& sfw, const map<WordID,int>& tfw, const Lattice& sourcelattice, int spanstart, int spanend);

  // Compute orientation model score which parameters are stored in *table* and pass the values accordingly
  // will call Orientation(Source|Target) and ScoreOrientation(Source|Target)
  void computeOrientationSource(const CountTable& table, double *cost, double *bonus, double *bo1, 
                                double *bo1_bonus, double *bo2, double *bo2_bonus);
  void computeOrientationSourcePos(const CountTable& table, double *cost, double *bonus,
                double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus, int maxfwidx, int maxdepth1, int maxdepth2);
  void computeOrientationSourceGen(const CountTable& table, double *cost, double *bonus, double *bo1, 
                                double *bo1_bonus, double *bo2, double *bo2_bonus, const map<WordID,WordID>& tags);
  void computeOrientationSourceBackward(const CountTable& table, double *cost, double *bonus, double *bo1, 
                                double *bo1_bonus, double *bo2, double *bo2_bonus);
  void computeOrientationSourceBackwardPos(const CountTable& table, double *cost, double *bonus, double *bo1, 
                                double *bo1_bonus, double *bo2, double *bo2_bonus, int maxfwidx, int maxdepth1, int maxdepth2);
  void computeOrientationTarget(const CountTable& table, double *cost, double *bonus, double *bo1, 
                                double *bo1_bonus, double *bo2, double *bo2_bonus);
  void computeOrientationTargetBackward(const CountTable& table, double *cost, double *bonus, double *bo1, 
                                double *bo1_bonus, double *bo2, double *bo2_bonus);
  // Get the orientation value of a function word at a particular index *fw*
  // assign the value to either *oril* or *orir* accoring to *Lcompute* and *Rcompute*
  void OrientationSource(int fw, int*oril, int* orir, bool Lcompute=true, bool Rcompute=true);
  void OrientationSource(int fw0, int fw1, int*oril, int* orir, bool Lcompute=true, bool Rcompute=true);
  int  OrientationSource(int* left, int* right);
  void OrientationTarget(int fw, int*oril, int* orir, bool Lcompute=true, bool Rcompute=true);
  void OrientationTarget(int fw0, int fw1, int*oril, int* orir, bool Lcompute=true, bool Rcompute=true);

  vector<int> OrientationSourceLeft4Sampler(int fw0, int fw1);
  vector<int> OrientationSourceLeft4Sampler(int fw);
  vector<int> OrientationSourceRight4Sampler(int fw0, int fw1);
  vector<int> OrientationSourceRight4Sampler(int fw);
  vector<int> OrientationTargetLeft4Sampler(int fw0, int fw1);
  vector<int> OrientationTargetLeft4Sampler(int fw);
  vector<int> OrientationTargetRight4Sampler(int fw0, int fw1);
  vector<int> OrientationTargetRight4Sampler(int fw);

  // Given an orientation value *ori*, estimate the score accoding to *cond1*, *cond2* 
  // and assign the value accordingly according to *isBonus* and whether the first or the second tier estimation
  // is used or not
  void ScoreOrientationRight(const CountTable& table, int ori, WordID cond1, WordID cond2, 
                             bool isBonus, double *cost, double *bonus, double *bo1, double *bo1_bonus, 
                             double *bo2, double *bo2_bonus, double alpha1, double beta1);
  void ScoreOrientationLeft(const CountTable& table, int ori, WordID cond1, WordID cond, 
                            bool isBonus, double *cost, double *bonus, double *bo1, double *bo1_bonus, 
                            double *bo2, double *bo2_bonus, double alpha1, double beta1);
  double ScoreOrientationRight(const CountTable& table, int ori, WordID cond1, WordID cond2); 
  double ScoreOrientationLeft(const CountTable& table, int ori, WordID cond1, WordID cond); 
  void ScoreOrientationRightBackward(const CountTable& table, int ori, WordID cond1, WordID cond2, 
                             bool isBonus, double *cost, double *bonus, double *bo1, double *bo1_bonus, 
                             double *bo2, double *bo2_bonus, double alpha1, double beta1);
  void ScoreOrientationLeftBackward(const CountTable& table, int ori, WordID cond1, WordID cond, 
                            bool isBonus, double *cost, double *bonus, double *bo1, double *bo1_bonus, 
                            double *bo2, double *bo2_bonus, double alpha1, double beta1);
  double ScoreOrientationRightBackward(const CountTable& table, int ori, WordID cond1, WordID cond2); 
  double ScoreOrientationLeftBackward(const CountTable& table, int ori, WordID cond1, WordID cond); 
  void ScoreOrientation(const CountTable& table, int offset, int ori, WordID cond1, WordID cond2, 
                            bool isBonus, double *cost, double *bonus, double *bo1, double *bo1_bonus, 
                            double *bo2, double *bo2_bonus, double alpha1, double beta1);
  double ScoreOrientation(const CountTable& table, int offset, int ori, WordID cond1, WordID cond2); 

  // idem as above except these are for dominance model
  void computeDominanceSource(const CountTable& table, WordID lfw, WordID rfw, double *cost, double *bonus, 
                              double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus);
  void computeDominanceSourcePos(const CountTable& table, WordID lfw, WordID rfw, double *cost, double *bonus, 
                              double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus, int maxfwidx, int maxdepth1, int maxdepth2);
  void computeDominanceTarget(const CountTable& table, WordID lfw, WordID rfw, double *cost, double *bonus, 
                              double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus);
  void computeBorderDominanceSource(const CountTable& table, double *cost, double *bonus, 
        double *state_mono, double *state_nonmono,
        TRule &rule, const std::vector<const void*>& ant_contexts, const map<WordID,int>& sfw);
  int DominanceSource(int fw1, int fw2);
  int DominanceTarget(int fw1, int fw2);
  vector<int> DominanceSource4Sampler(int fw1, int fw2);
  vector<int> DominanceTarget4Sampler(int fw1, int fw2);
  void ScoreDominance(const CountTable& table, int dom, WordID s1, WordID s2, WordID t1, WordID t2, 
                      double *cost, double *bo1, double *bo2, bool isBonus, double alpha2, double beta2);
  double ScoreDominance(const CountTable& table, int dom, WordID s1, WordID s2, WordID t1, WordID t2); 

  // Remove all function word alignments except those at the borders
  // May result in more than two function word alignments at each side, because this function 
  // will continue keeping function word alignments until the first aligned word at each side
  void BorderingSFWsOnly();
  void BorderingTFWsOnly();
  void simplify(int *ret); // preparing the next state
  void simplify_nofw(int *ret); // preparing the next state when no function word appears
  // set the first part of the next state, which concerns with function word
  // fas, las, fat, lat is the (f)irst or (l)ast function word alignments either on the (s)ource or (t)arget
  // these parameters to anticipate cases where there are more than two function word alignments
  void FillFWIdxsState(int *state, int fas, int las, int fat, int lat);

  // Helper function to obtain the aligned words on the other side 
  // WARNING!!! Only to be used if the als are in sync with either source or target sentences
  WordID F2EProjectionFromExternal(int idx, const vector<AlignmentPoint>& als, const string& delimiter=" ");
  WordID E2FProjectionFromExternal(int idx, const vector<AlignmentPoint>& als, const string& delimiter=" ");
  // WARNING!!! Only to be used in dwarf_main.cc 
  // These two function words assume that the alignment contains phrase boundary 
  // but the source and target sentences do not
  WordID F2EProjection(int idx, const string& delimiter=" ");
  WordID E2FProjection(int idx, const string& delimiter=" ");
  void SetCurrAlVector();
  int* blockSource(int fw1, int fw2);
  int* blockTarget(int fw1, int fw2);
  void ToArrayInt(vector<int>* arr);
  int* neighborLeft(int startidx, int endidx, bool* found);
  int* neighborRight(int startidx, int endidx, bool* found);
private:
  // Hash to avoid redundancy
  unordered_map<vector<int>, int, boost::hash<vector<int> > > oris_hash;
  unordered_map<vector<int>, int, boost::hash<vector<int> > > orit_hash;
  unordered_map<vector<int>, int, boost::hash<vector<int> > > doms_hash;
  unordered_map<vector<int>, int, boost::hash<vector<int> > > domt_hash;
  unordered_map<vector<int>, vector<int>, boost::hash<vector<int> > > simplify_hash;
  unordered_map<vector<int>, vector<int>, boost::hash<vector<int> > > prepare_hash;
 
  int _J; // effective source length;
  int _I; // effective target length;
  bool _matrix[MAX_WORDS][MAX_WORDS]; // true if aligned 
  short _sSpan[MAX_WORDS][2]; //the source span of a target index; 0->min, 1->max
  short _tSpan[MAX_WORDS][2]; //the target span of a source index; 0->min, 2->max
  int _freq_cutoff;
  int SourceFWRuleIdxs[40]; //the indexes of function words in the rule; 
          // The following applies to all *FW*Idxs
          // *FW*Idxs[0] = size
          // *FW*Idxs[idx*3-2] = index in the alignment, where idx starts from 1 to size
          // *FW*Idxs[idx*3-1] = source WordID
          // *FW*Idxs[idx*3]   = target WordID
  int SourceFWRuleAbsIdxs[40];
  int TargetFWRuleIdxs[40]; //the indexes of function words in the rule; zeroth element is the count
  int ** SourceFWAntsIdxs;  //the indexes of function words in antecedents
  int ** SourceFWAntsAbsIdxs;
  int ** TargetFWAntsIdxs;  //the indexes of function words in antecedents
  int SourceRuleIdxs[40]; //the indexes of SOURCE tokens (zeroth element is the number of source tokens)
        //>0 means terminal, -i means the i-th Xs
  int TargetRuleIdxs[40]; //the indexes of TARGET tokens (zeroth element is the number of target tokens)
  int ** SourceAntsIdxs;  //the array of indexes of a particular antecedent's SOURCE tokens
  int ** TargetAntsIdxs;  //the array of indexes of a particular antecedent's TARGET tokens
  int SourceFWIdxs[40];
  int SourceFWAbsIdxs[40];
  int TargetFWIdxs[40];
  // *sort* and *quickSort* are used to sort *FW*Idxs
  void sort(int* num);
  void quickSort(int arr[], int top, int bottom);

  // *block(Source|Target)* finds the minimum block that containts two indexes (fw1 and fw2)
  inline int least(int i1, int i2) { return (i1<i2)?i1:i2; }
  inline int most(int i1, int i2) { return (i1>i2)?i1:i2; }
  void simplifyBackward(vector<int *>*blocks, int* block, const vector<int>& danglings);
  // used in simplify to check whether an atomic block according to source function words is also atomic according
  // to target function words as well, otherwise break it 
  // the resulting blocks are added into *blocks*
  int _Arity;
  std::vector<WordID> _f; // the source sentence of the **current** rule (may not consistent with the current alignment)
  std::vector<WordID> _e; // the target sentence of the **current** rule
  int RuleAl[40];
  int **AntsAl;
  int firstSourceAligned(int start);
  int firstTargetAligned(int start);
  int lastSourceAligned(int end);
  int lastTargetAligned(int end);
  int fas, las, fat, lat; // first aligned source, last aligned source, first aligned target, last aligned target
  bool MemberOf(int* FWIdxs, int pos1, int pos2); // whether FWIdxs contains pos1 and pos2 consecutively
  // Convert the alignment to vector form, will be used for hashing purposes
  vector<int> curr_al;
  int GetFWGlobalIdx(int idx, const Lattice& sourcelattice, vector<WordID>& sources, int spanstart, int spanend, const std::vector<const void*>& ant_contexts, const map<WordID,int>& sfw);
  int GetFirstFWIdx(int spanstart,int spanend, const Lattice& sourcelattice, const map<WordID,int>& sfw);
  int GetLastFWIdx(int spanstart,int spanend, const Lattice& sourcelattice, const map<WordID,int>& sfw);
  WordID generalize(WordID original, const map<WordID,WordID>& tags, bool pos=false);
};

#endif
