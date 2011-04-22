#include "dwarf.h"
#include "tdict.h"
#include "wordid.h"
#include "lattice.h"
#include "ff_dwarf.h"
#include <assert.h>
#include <algorithm>
#include <ostream>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <boost/functional/hash.hpp>
#include <tr1/unordered_map>
#include <boost/tuple/tuple.hpp>

using namespace std;
using namespace std::tr1;
using namespace boost::tuples;
using namespace boost;

Alignment::Alignment() {
  //unordered_map<std::vector<WordID>,int> XX;
  _I=0;
  _J=0;
  kSOS = TD::Convert("<s>");
  kEOS = TD::Convert("</s>");
  kUNK = TD::Convert("**UNKNOWN**");
  SourceFWAntsIdxs = new int*[MAX_ARITY];
  SourceFWAntsAbsIdxs = new int*[MAX_ARITY];
  TargetFWAntsIdxs = new int*[MAX_ARITY];
  SourceAntsIdxs = new int*[MAX_ARITY];
  TargetAntsIdxs = new int*[MAX_ARITY];
  AntsAl = new int*[MAX_ARITY];
  for (int idx=0; idx<MAX_ARITY; idx++) {
    SourceAntsIdxs[idx] = new int[40];
    SourceFWAntsIdxs[idx] = new int[40];
    SourceFWAntsAbsIdxs[idx] = new int[40];
    TargetAntsIdxs[idx] = new int[40];
    TargetFWAntsIdxs[idx] = new int[40];
    AntsAl[idx] = new int[40];
  }    
  for (int j=0; j<MAX_WORDS; j++) 
    for (int i=0; i<MAX_WORDS; i++) _matrix[j][i]=false; 
  for (int j=0; j<MAX_WORDS; j++) {
    _tSpan[j][0]=MINIMUM_INIT;
    _sSpan[j][1]=MAXIMUM_INIT;
  }
  for (int i=0; i<MAX_WORDS; i++) {
    _sSpan[i][0]=MINIMUM_INIT;
    _sSpan[i][1]=MAXIMUM_INIT;
  }
  alpha_oris=0.1;
  alpha_orit=0.1;
  alpha_doms=0.1;
  alpha_domt=0.1;
  beta_oris=0.1;
  beta_orit=0.1;
  beta_doms=0.1;
  beta_domt=0.1;
}

void Alignment::set(int j,int i) {
// create a link between j and i, update their corresponding span accordingly
  if (DEBUG) cerr << "set(" << j << "," << i << ")" << endl;
  assert(0<=j && j<MAX_WORDS);
  assert(0<=i && i<MAX_WORDS);
  if (0<=j && j<MAX_WORDS && 0<=i && i<MAX_WORDS) {
    _matrix[j][i] = true;
    _tSpan[j][0]=least(i,_tSpan[j][0]);
    _tSpan[j][1]=most(i,_tSpan[j][1]);
    _sSpan[i][0]=least(j,_sSpan[i][0]);
    _sSpan[i][1]=most(j,_sSpan[i][1]);
  }
  _J=most(j+1,_J);
  _I=most(i+1,_I);
}

void Alignment::reset(int j,int i) { //probably won't be used, since the alignment is not dynamic
// remove the link between j and i, update their corresponding span accordingly
  if (DEBUG) cerr << "reset(" << j << "," << i << ")" << endl;
  assert(0<=j && j<MAX_WORDS);
  assert(0<=i && i<MAX_WORDS);
  _matrix[j][i] = false;
  if (j==_sSpan[i][0] || j==_sSpan[i][1]) {
    int min=MINIMUM_INIT;
    int max=MAXIMUM_INIT;
    for (int idx=_sSpan[i][0]; idx<=_sSpan[i][1]; idx++) {
      if (_matrix[idx][i]) {
        min=least(min,idx);
        max=most(max,idx);
      }
    }
    _sSpan[i][0]=min;
    _sSpan[i][1]=max;
  }
  if (i==_tSpan[j][0] || i==_tSpan[j][1]) {
    int min=MINIMUM_INIT;
    int max=MAXIMUM_INIT;
    for (int idx=_tSpan[j][0]; idx<=_tSpan[j][1]; idx++) {
      if (_matrix[j][idx]) {
        min=least(min,idx);
        max=most(max,idx);
      }
    }
    _tSpan[j][0]=min;
    _tSpan[j][1]=max;
  }
}

int Alignment::targetOf(int j, int start) {
  assert(j>=0);
  if (start==-1) start = _tSpan[j][0];
  if (_tSpan[j][0]==MINIMUM_INIT) return -1;
  for (int idx=start; idx<=_tSpan[j][1]; idx++) {
    if (_matrix[j][idx]) return idx;
  }
  return -1;
}

int Alignment::sourceOf(int i, int start) {
  assert(i>=0);
  if (start==-1) start = _sSpan[i][0];
  if (_sSpan[i][0]==MINIMUM_INIT) return -1;
  for (int idx=start; idx<=_sSpan[i][1]; idx++) {
    if (_matrix[idx][i]) return idx;
  }
  return -1;
}

void Alignment::clearAls(int prevJ, int prevI) {
  for (int j=0; j<=prevJ; j++) {
    for (int i=0; i<prevI; i++) {
      _matrix[j][i]=false;
    }
  }
  for (int j=0; j<=prevJ; j++) {
    _tSpan[j][0] = MINIMUM_INIT;
    _tSpan[j][1] = MAXIMUM_INIT;
  }
  for (int i=0; i<=prevI; i++) {
    _sSpan[i][0] = MINIMUM_INIT;
    _sSpan[i][1] = MAXIMUM_INIT;
  }
  _J=0;
  _I=0;
}

int Alignment::DominanceSource(int fw1, int fw2) {
  // Dominance of fw1 and fw2 
  // 0 -> neither, 1 -> leftFirst, 2 -> rightFirst, 3 -> dontCare
  if (DEBUG) cerr << "DominanceSource(" << fw1 << "," << fw2 << ")" << endl;
  //cerr << TD::Convert(_f[fw1]) << "," << TD::Convert(_f[fw2]) << endl; 
  //cerr << AsString() << endl;
  int dom = 0;
  curr_al.push_back(fw1); curr_al.push_back(fw2);
  if (doms_hash.find(curr_al)==doms_hash.end()) {
    int* block = blockSource(fw1,fw2);
    //cerr << "block = " << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
    if (block[0]==fw1) { 
      int tfw10 = _tSpan[fw1][0];
      int tfw11 = _tSpan[fw1][1];
      //cerr << "tfw = " << tfw10 << "," << tfw11 << endl;
      if (tfw11<0) { 
        dom+=1;
      } else {
        if ((block[2]==tfw10 || block[3]==tfw11)) dom+=1;
      }
    }
    if (block[1]==fw2) {
      int tfw20 = _tSpan[fw2][0];
      int tfw21 = _tSpan[fw2][1];
      //cerr << "tfw = " << tfw20 << "," << tfw21 << endl;
      if (tfw21<0) {
        dom+=2;
      } else {
        if ((block[2]==tfw20 || block[3]==tfw21)) dom+=2;
      }
    }
    delete block;
    doms_hash.insert(pair<vector<int>,int>(curr_al,dom));
  } else {
    dom = doms_hash[curr_al];
  }
  if (DEBUG) cerr << "  dom = " << dom << endl;
  curr_al.pop_back(); curr_al.pop_back();
  return dom;
}

vector<int> Alignment::DominanceSource4Sampler(int fw1, int fw2) {
  if (DEBUG) cerr << "DominanceSource4Sampler(" << fw1 << "," << fw2 << ")" << endl;
  int dom = 0;
  int* block = blockSource(fw1,fw2);
  //cerr << "block = " << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
  if (block[0]==fw1) {
    int tfw10 = _tSpan[fw1][0];
    int tfw11 = _tSpan[fw1][1];
    //cerr << "tfw = " << tfw10 << "," << tfw11 << endl;
    if (tfw11<0) {
      dom+=1;
    } else {
      if ((block[2]==tfw10 || block[3]==tfw11)) dom+=1;
    }
  }
  if (block[1]==fw2) {
    int tfw20 = _tSpan[fw2][0];
    int tfw21 = _tSpan[fw2][1];
    //cerr << "tfw = " << tfw20 << "," << tfw21 << endl;
    if (tfw21<0) {
      dom+=2;
    } else {
      if ((block[2]==tfw20 || block[3]==tfw21)) dom+=2;
    }
  }
  if (DEBUG) cerr << "doms = " << dom << endl;
  vector<int> ret;
  ret.push_back(dom); ret.push_back(block[0]); ret.push_back(block[1]);
  ret.push_back(block[2]); ret.push_back(block[3]);
  delete block;
  return ret;
}

int Alignment::DominanceTarget(int fw1, int fw2) {
  int dom = 0;
  curr_al.push_back(fw1); curr_al.push_back(fw2);
  if (domt_hash.find(curr_al)==domt_hash.end()) {
    int* block = blockTarget(fw1,fw2);
    if (block[2]==fw1) {
      int sfw10 = _sSpan[fw1][0];
      int sfw11 = _sSpan[fw1][1];
      if (sfw11<0) {
        dom+=1;
      } else {
        if (block[0]==sfw10 || block[1]==sfw11) dom+=1;
      }
    }
    if (block[3]==fw2) {
      int sfw20 = _sSpan[fw2][0];
      int sfw21 = _sSpan[fw2][0];
      if (sfw21<0) {
        dom+=2;
      } else {
        if (block[0]==sfw20 || block[1]==sfw21) dom+=2;
      }
    }
    delete block;
    domt_hash.insert(pair<vector<int>,int>(curr_al,dom));
  } else {
    dom = domt_hash[curr_al];
  }
  curr_al.pop_back(); curr_al.pop_back();
  return dom;
}

vector<int> Alignment::DominanceTarget4Sampler(int fw1, int fw2) {
  int dom = 0;
  int* block = blockTarget(fw1,fw2);
  if (block[2]==fw1) {
    int sfw10 = _sSpan[fw1][0];
    int sfw11 = _sSpan[fw1][1];
    if (sfw11<0) {
      dom+=1;
    } else {
      if (block[0]==sfw10 || block[1]==sfw11) dom+=1;
    }
  }
  if (block[3]==fw2) {
    int sfw20 = _sSpan[fw2][0];
    int sfw21 = _sSpan[fw2][0];
    if (sfw21<0) {
      dom+=2;
    } else {
      if (block[0]==sfw20 || block[1]==sfw21) dom+=2;
    }
  }
  vector<int> ret;
  ret.push_back(dom); ret.push_back(block[0]); ret.push_back(block[1]);
  ret.push_back(block[2]); ret.push_back(block[3]);
  delete block;
  return ret;
}

void Alignment::OrientationSource(int fw, int* oril, int* orir, bool Lcompute, bool Rcompute) {
  OrientationSource(fw,fw,oril,orir,Lcompute,Rcompute);
}

vector<int> Alignment::OrientationSourceLeft4Sampler(int fw) {
  return OrientationSourceLeft4Sampler(fw,fw);
}

vector<int> Alignment::OrientationSourceLeft4Sampler(int fw0, int fw1) {
  if (DEBUG) cerr << "OrientationSourceLeft4Sampler(" << fw0 << "," << fw1 << ")" << endl;
  int oril = 0;
  int N0=fw0-1;
  while (N0>=0) {
    if (minTSpan(N0)!=MINIMUM_INIT) break;
    N0--;
  }
  int N1=fw1+1;
  while (N1<_J) {
    if (minTSpan(N1)!=MINIMUM_INIT) break;
    N1++;
  }
  if (minTSpan(fw0)==MINIMUM_INIT && minTSpan(fw1)==MINIMUM_INIT) {
    fw0 = N1; fw1 = N0;
  }
  if (DEBUG) cerr << "fw0=" << fw0 << ", fw1=" << fw1 << ", N0=" << N0 << ", N1=" << N1 << endl;
  if (maxTSpan(N0)<minTSpan(fw0) || maxTSpan(fw0)<minTSpan(N0)) {
    if (DEBUG) cerr << "N0=" << minTSpan(N0) << "-" << maxTSpan(N0);
    if (DEBUG) cerr << "fw=" << minTSpan(fw0) << "-" << maxTSpan(fw0) << endl;
    int *block = blockTarget(minTSpan(N0),maxTSpan(N0));
    if (block[0]<=fw0 && fw0<=block[1]) oril=5;
    delete block;
    if (oril==0) {
      block = blockTarget(minTSpan(fw0),maxTSpan(fw0));
      if (block[0]<=N0 && N0<=block[1]) oril=5;
      delete block;
    }
    if (oril==0) {
      if (maxTSpan(N0)<minTSpan(fw0)) {// if N0 is monotone
        oril=1;
        block = blockTarget(maxTSpan(N0),minTSpan(fw0)-1);
        if (block[0] <= fw0 && fw0 <= block[1]) oril+=2;
        delete block;
      } else { //if (maxTSpan(fw0)<minTSpan(N0)) { // if NO is non-monotone
        oril=2;
        block = blockTarget(maxTSpan(fw0)+1,minTSpan(N0));
        if (block[0] <= fw0 && fw0 <= block[1]) oril+=2;
        delete block;
      }
    }
  } else {
    oril=5;
  }
  if (DEBUG) cerr << "oril = " << oril << endl;
  int* block = blockSource(N0,fw0);
  if (DEBUG) {
    for (int i=0; i<4; i++) cerr << "block[" << i << "]=" << block[i] << endl;
  }
  vector<int> ret;
  ret.push_back(oril); ret.push_back(block[0]); ret.push_back(block[1]);
  ret.push_back(block[2]); ret.push_back(block[3]);
  delete block;
  return ret;
}

vector<int> Alignment::OrientationSourceRight4Sampler(int fw) {
  return OrientationSourceRight4Sampler(fw,fw);
}

vector<int> Alignment::OrientationSourceRight4Sampler(int fw0, int fw1) {
  if (DEBUG) cerr << "OrientationSourceLeft4Sampler(" << fw0 << "," << fw1 << ")" << endl;
  int orir = 0;
  int N0=fw0-1;
  while (N0>=0) {
    if (minTSpan(N0)!=MINIMUM_INIT) break;
    N0--;
  }
  int N1=fw1+1;
  while (N1<_J) {
    if (minTSpan(N1)!=MINIMUM_INIT) break;
    N1++;
  }
  if (minTSpan(fw0)==MINIMUM_INIT && minTSpan(fw1)==MINIMUM_INIT) {
    fw0 = N1; fw1 = N0;
  }
  if (DEBUG) cerr << "fw0=" << fw0 << ", fw1=" << fw1 << ", N0=" << N0 << ", N1=" << N1 << endl;
  if (maxTSpan(N1)<minTSpan(fw1) || maxTSpan(fw1)<minTSpan(N1)) {
    int* block = blockTarget(minTSpan(N1),maxTSpan(N1));
    if (block[0]<=fw1 && fw1<=block[2]) orir=5;
    delete block;
    if (orir==0) {
      block = blockTarget(minTSpan(fw1),maxTSpan(fw1));
      if (block[0]<=N1 && N1 <=block[1]) orir=5;
      delete block;
    }
    if (DEBUG) cerr << "N1=" << minTSpan(N1) << "-" << maxTSpan(N1);
    if (DEBUG) cerr << "fw1=" << minTSpan(fw1) << "-" << maxTSpan(fw1) << endl;
    if (orir==0) {
      if (maxTSpan(fw1)<minTSpan(N1)) { // if N1 is monotone
        orir = 1;
        block = blockTarget(maxTSpan(fw1)+1,minTSpan(N1));
        if (block[0] <= fw1 && fw1 <= block[1]) orir+=2;
        delete block;
      } else {// if (maxTSpan(N1)<minTSpan(fw1)) { // if N1 is non-monotone
        orir = 2;
        block = blockTarget(maxTSpan(N1),minTSpan(fw1)-1);
        if (block[0] <= fw1 && fw1 <= block[1]) orir+=2;
        delete block;
      }
    }
  } else {
    orir = 5;
  }
  if (DEBUG) cerr << "orir = " << orir << endl;
  int* block = blockSource(fw1,N1);
  vector<int> ret;
  ret.push_back(orir); ret.push_back(block[0]); ret.push_back(block[1]);
  ret.push_back(block[2]); ret.push_back(block[3]);
  delete block;
  return ret;
}

void Alignment::OrientationSource(int fw0, int fw1, int* oril, int* orir, bool Lcompute, bool Rcompute) {
  // Orientation
  // A bit tricky since fw can be 1) unaligned 2) aligned to many
  // 1 -> MA, 2 -> RA, 3 -> MG, 4 -> RG, 5 -> Other
  if (DEBUG) cerr << "OrientationSource(" << fw0 << "," << fw1 << ")" << endl;
  if (!Lcompute && !Rcompute) return;
  curr_al.push_back(fw0);
  curr_al.push_back(fw1);
  *oril=0;
  *orir=0;
  int lr=0;
  if (oris_hash.find(curr_al)==oris_hash.end()) {
    // Find first aligned word N0 to the left of fw
    int N0=fw0-1;
    while (N0>=0) {
      if (minTSpan(N0)!=MINIMUM_INIT) break;
      N0--;
    }
    int N1=fw1+1;
    while (N1<_J) {
      if (minTSpan(N1)!=MINIMUM_INIT) break;
      N1++;
    }
    if (minTSpan(fw0)==MINIMUM_INIT && minTSpan(fw1)==MINIMUM_INIT) {
      fw0 = N1; fw1 = N0;
      //cerr << "minTSpan(fw)==MINIMUM_INIT, thus fw0=" << fw0 << ", fw1=" << fw1 << endl;
    }
    if (DEBUG) cerr << "fw0=" << fw0 << ", fw1=" << fw1 << ", N0=" << N0 << ", N1=" << N1 << endl;
    if (maxTSpan(N0)<minTSpan(fw0) || maxTSpan(fw0)<minTSpan(N0)) {
      if (DEBUG) cerr << "N0=" << minTSpan(N0) << "-" << maxTSpan(N0);
      if (DEBUG) cerr << "fw=" << minTSpan(fw0) << "-" << maxTSpan(fw0) << endl;
      int *block = blockTarget(minTSpan(N0),maxTSpan(N0));
      if (block[0]<=fw0 && fw0<=block[1]) *oril=5;
      delete block;
      if (*oril==0) {
        block = blockTarget(minTSpan(fw0),maxTSpan(fw0));
        if (block[0]<=N0 && N0<=block[1]) *oril=5;
        delete block;
      }
      if (*oril==0) {
        if (maxTSpan(N0)<minTSpan(fw0)) {// if N0 is monotone
          *oril=1;
          block = blockTarget(maxTSpan(N0),minTSpan(fw0)-1);
          if (block[0] <= fw0 && fw0 <= block[1]) *oril+=2;
          delete block;
        } else { //if (maxTSpan(fw0)<minTSpan(N0)) { // if NO is non-monotone
          *oril=2;
          block = blockTarget(maxTSpan(fw0)+1,minTSpan(N0));
          if (block[0] <= fw0 && fw0 <= block[1]) *oril+=2;
          delete block;
        }
      }
    } else {
      *oril=5;
    }
    if (DEBUG) cerr << "oril =" << *oril << endl;
    // Right neighbor
    if (maxTSpan(N1)<minTSpan(fw1) || maxTSpan(fw1)<minTSpan(N1)) {
      int* block = blockTarget(minTSpan(N1),maxTSpan(N1));
      if (block[0]<=fw1 && fw1<=block[2]) *orir=5;
      delete block;
      if (*orir==0) {
        block = blockTarget(minTSpan(fw1),maxTSpan(fw1));
        if (block[0]<=N1 && N1 <=block[1]) *orir=5;
        delete block;
      }
      if (DEBUG) cerr << "N1=" << minTSpan(N1) << "-" << maxTSpan(N1);
      if (DEBUG) cerr << "fw1=" << minTSpan(fw1) << "-" << maxTSpan(fw1) << endl;
      if (*orir==0) {
        if (maxTSpan(fw1)<minTSpan(N1)) { // if N1 is monotone
          *orir = 1;
          block = blockTarget(maxTSpan(fw1)+1,minTSpan(N1));
          if (block[0] <= fw1 && fw1 <= block[1]) *orir+=2;
          delete block;
        } else {// if (maxTSpan(N1)<minTSpan(fw1)) { // if N1 is non-monotone
          *orir = 2;
          block = blockTarget(maxTSpan(N1),minTSpan(fw1)-1);
          if (block[0] <= fw1 && fw1 <= block[1]) *orir+=2;
          delete block;
        }
      }
    } else {
      *orir = 5;
    }
    if (DEBUG) cerr << "orir =" << *orir << endl;
    lr = link(*oril,*orir);
    oris_hash.insert(pair<vector<int>,int>(curr_al,lr));  
  } else {
    lr = oris_hash[curr_al];  
  }
  if (DEBUG) cerr << "Lcompute=" << Lcompute << ", Rcompute=" << Rcompute << endl;
  if (Lcompute) *oril = source(lr);
  if (Rcompute) *orir = target(lr);
  curr_al.pop_back();
  curr_al.pop_back();
}

int Alignment::OrientationSource(int* left, int* right) {
  if (DEBUG) {
    cerr << "      OrientationSource(";
    cerr << "left="<<left[0]<<","<<left[1]<<","<<left[2]<<","<<left[3];
    cerr << " right="<<right[0]<<","<<right[1]<<","<<right[2]<<","<<right[3];
    cerr << ")" << endl;
  } 
  //if ((right[1]<=left[0]) return 5;
  if (!(left[1]<right[0])) return 5;
  int ori = 1;
  if (right[3]<left[2]) ori=2;
  int gapstart = left[3]+1; int gapend = right[2]-1;
  if (ori==2) { gapstart = right[3]+1; gapend = left[2]-1; }
  for (int j=gapstart; j<=gapend; j++) {
    if (sourceOf(j)!=-1) {
      ori+=2; break;
    }
  }
  return ori;
}

void Alignment::OrientationTarget(int fw, int *oril, int *orir, bool Lcompute, bool Rcompute) {
  OrientationTarget(fw,fw,oril,orir,Lcompute,Rcompute);
}

vector<int> Alignment::OrientationTargetLeft4Sampler(int fw) {
  return OrientationTargetLeft4Sampler(fw,fw);
}

vector<int> Alignment::OrientationTargetLeft4Sampler(int fw0, int fw1) {
  if (DEBUG) cerr << "OrientationTargetLeft4Sampler " << fw0 << "," << fw1 << endl;
  int oril=0; 
  int N0=fw0-1;
  while (N0>=0) {
    if (minSSpan(N0)!=MINIMUM_INIT) break;
    N0--;
  }
  int N1=fw1+1;
  while (N1<_I) {
    if (minSSpan(N1)!=MINIMUM_INIT) break;
    N1++;
  }
  if (minSSpan(fw0)==MINIMUM_INIT && minSSpan(fw1)==MINIMUM_INIT) {
    fw0=N1; fw1=N0;
  }
  if (maxSSpan(N0)<minSSpan(fw0) || maxSSpan(fw0)<minSSpan(N0)) {
    int *block = blockSource(minSSpan(N0),maxSSpan(N0));
    if (DEBUG) cerr << "block1[" << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
    if (block[2]<=fw0 && fw0<=block[3])  //source span of fw0 subsumes NO's or the other way around
      oril=5;
    delete block;
    if (oril==0) {
      block = blockSource(minSSpan(fw0), maxSSpan(fw0));
      if (DEBUG) cerr << "block2[" << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
      if (block[2] <= N0 && N0 <= block[3]) oril=5;
      delete block;
    }
    if (oril==0) {
      if (maxSSpan(N0)<minSSpan(fw0)) {// if N0 is monotone
        oril=1;
        block = blockSource(maxSSpan(N0),minSSpan(fw0)-1);
        if (DEBUG) cerr << "block3[" << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
        if (block[2] <= fw0 && fw0 <= block[3]) oril+=2;
        delete block;
      } else { // (maxSSpan(fw0)<minSSpan(N0)) // if NO is non-monotone
        oril=2;
        block = blockSource(maxSSpan(fw0)+1,minSSpan(N0));
        if (DEBUG) cerr << "block4[" << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
        if (block[2] <= fw0 && fw0 <= block[3]) oril+=2;
        delete block;
      }
    }
  } else { //source span of fw0 subsumes NO's or the other way around
    oril=5;
  }
  if (DEBUG) cerr << "oril = " << oril << endl;
  int* block = blockSource(N0,fw0);
  vector<int> ret;
  ret.push_back(oril); ret.push_back(block[0]); ret.push_back(block[1]);
  ret.push_back(block[2]); ret.push_back(block[3]);
  delete block;
  return ret;
}

vector<int> Alignment::OrientationTargetRight4Sampler(int fw) {
  return OrientationTargetRight4Sampler(fw,fw);
}

vector<int> Alignment::OrientationTargetRight4Sampler(int fw0, int fw1) {
  if (DEBUG) cerr << "OrientationTargetRight4Sampler " << fw0 << "," << fw1 << endl;
  int orir=0;
  int N0=fw0-1;
  while (N0>=0) {
    if (minSSpan(N0)!=MINIMUM_INIT) break;
    N0--;
  }
  int N1=fw1+1;
  while (N1<_I) {
    if (minSSpan(N1)!=MINIMUM_INIT) break;
    N1++;
  }
  if (minSSpan(fw0)==MINIMUM_INIT && minSSpan(fw1)==MINIMUM_INIT) {
    fw0=N1; fw1=N0;
  }
  if (maxSSpan(N1)<minSSpan(fw1) || maxSSpan(fw1)<minSSpan(N1)) {
    int *block = blockSource(minSSpan(N1),maxSSpan(N1));
    if (block[2]<=fw1 && fw1<=block[3]) orir=5;
    delete block;
    if (orir==0) {
      block = blockSource(minSSpan(fw1),maxSSpan(fw1));
      if (block[2] <= N1 && N1 <= block[3]) orir=5;
      delete block;
    }
    if (orir==0) {
      if (maxSSpan(fw1)<minSSpan(N1)) { // if N1 is monotone
        orir=1;
        block = blockSource(maxSSpan(fw1)+1,minSSpan(N1));
        if (block[2] <= fw1 && fw1 <= block[3]) orir+=2;
        delete block;
      } else { //if (maxSSpan(N1)<minSSpan(fw1)) { // if N1 is non-monotone
        orir=2;
        block = blockSource(maxSSpan(N1),minSSpan(fw1)-1);
        if (block[2] <= fw1 && fw1 <= block[3]) orir+=2;
        delete block;
      }
    }
  } else {
    orir=5;
  }
  if (DEBUG) cerr << "orir = " << orir << endl;
  int* block = blockSource(fw1,N1);
  vector<int> ret;
  ret.push_back(orir); ret.push_back(block[0]); ret.push_back(block[1]);
  ret.push_back(block[2]); ret.push_back(block[3]);
  delete block;
  return ret;

}

void Alignment::OrientationTarget(int fw0, int fw1, int*oril, int*orir, bool Lcompute, bool Rcompute) {
  if (DEBUG) cerr << "OrientationTarget " << fw0 << "," << fw1 << endl;
  // Left Neighbor
  if (!Lcompute && !Rcompute) return;
  *oril=0;
  *orir=0;
  curr_al.push_back(fw0);
  curr_al.push_back(fw1);
  int lr = 0;
  if (orit_hash.find(curr_al)==orit_hash.end()) {
    // Find first aligned word N0 to the left of fw
    //int fw0 = fw; int fw1 = fw;
    int N0=fw0-1;
    while (N0>=0) {
      if (minSSpan(N0)!=MINIMUM_INIT) break;
      N0--;
    }
    int N1=fw1+1;
    while (N1<_I) {
      if (minSSpan(N1)!=MINIMUM_INIT) break;
      N1++;
    }
    if (minSSpan(fw0)==MINIMUM_INIT && minSSpan(fw1)==MINIMUM_INIT) {
      fw0=N1; fw1=N0;
    }
    if (DEBUG) {
      cerr << "fw0:" << fw0 << ", fw1:" << fw1 << ", N0:" << N0 << ", N1:" << N1 << endl ;
      cerr << "minSSpan(N0)=" << minSSpan(N0) << " maxSSpan(N0)=" << maxSSpan(N0);
      cerr << " minSSpan(fw0)="<< minSSpan(fw0) << " maxSSpan(fw0)=" << maxSSpan(fw0) << endl;
      cerr << "minSSpan(fw1)=" << minSSpan(fw1) << " maxSSpan(fw1)=" << maxSSpan(fw1);
      cerr << " minSSpan(N1)="<< minSSpan(N1) << " maxSSpan(N1)=" << maxSSpan(N1) << endl;
    }
    if (maxSSpan(N0)<minSSpan(fw0) || maxSSpan(fw0)<minSSpan(N0)) {
      int *block = blockSource(minSSpan(N0),maxSSpan(N0));
      if (DEBUG) cerr << "block1[" << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
      if (block[2]<=fw0 && fw0<=block[3])  //source span of fw0 subsumes NO's or the other way around
        *oril=5;
      delete block;
      if (*oril==0) {
        block = blockSource(minSSpan(fw0), maxSSpan(fw0));
        if (DEBUG) cerr << "block2[" << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
        if (block[2] <= N0 && N0 <= block[3]) *oril=5;
        delete block;
      }
      if (*oril==0) {  
        if (maxSSpan(N0)<minSSpan(fw0)) {// if N0 is monotone
          *oril=1;
          block = blockSource(maxSSpan(N0),minSSpan(fw0)-1);
          if (DEBUG) cerr << "block3[" << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
          if (block[2] <= fw0 && fw0 <= block[3]) *oril+=2;
          delete block;
        } else { // (maxSSpan(fw0)<minSSpan(N0)) // if NO is non-monotone
          *oril=2;
          block = blockSource(maxSSpan(fw0)+1,minSSpan(N0));
          if (DEBUG) cerr << "block4[" << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
          if (block[2] <= fw0 && fw0 <= block[3]) *oril+=2;
          delete block;
        }
      }
    } else { //source span of fw0 subsumes NO's or the other way around
      *oril=5;
    }
    if (DEBUG) cerr << "oril = " << *oril << endl;
    // Right Neighbor
    if (maxSSpan(N1)<minSSpan(fw1) || maxSSpan(fw1)<minSSpan(N1)) {
      int *block = blockSource(minSSpan(N1),maxSSpan(N1));
      if (block[2]<=fw1 && fw1<=block[3]) *orir=5; 
      delete block;
      if (*orir==0) {
        block = blockSource(minSSpan(fw1),maxSSpan(fw1));
        if (block[2] <= N1 && N1 <= block[3]) *orir=5;
        delete block;
      }
      if (*orir==0) {
        if (maxSSpan(fw1)<minSSpan(N1)) { // if N1 is monotone
          *orir=1;
          block = blockSource(maxSSpan(fw1)+1,minSSpan(N1));
          if (block[2] <= fw1 && fw1 <= block[3]) *orir+=2;
          delete block;
        } else { //if (maxSSpan(N1)<minSSpan(fw1)) { // if N1 is non-monotone
          *orir=2;
          block = blockSource(maxSSpan(N1),minSSpan(fw1)-1);
          if (block[2] <= fw1 && fw1 <= block[3]) *orir+=2;
          delete block;
        }
      }
    } else {
      *orir=5;
    }
    if (DEBUG) cerr << "orir = " << *orir << endl;
    lr = link(*oril,*orir);
    orit_hash.insert(pair<vector<int>,int>(curr_al,lr));
  } else {
    lr = orit_hash[curr_al];
  }
  if (DEBUG) cerr << "Lcompute=" << Lcompute << ", Rcompute=" << Rcompute << endl;
  if (DEBUG) cerr << "lr=" << lr << ", l=" << source(lr) << ", r=" << target(lr) << endl;
  if (Lcompute>0) *oril=source(lr);
  if (Rcompute>0) *orir=target(lr);
  curr_al.pop_back();
  curr_al.pop_back();
}

int* Alignment::blockSource(int idx1, int idx2) {
// outputs a minimal block [s1,s2,t1,t2] that contains idx1 and idx2, where idx1 <= idx2
  if (DEBUG) cerr << "blockSource[" << idx1 << "," << idx2 << "]" << endl;
  int *curr = new int[4];
  curr[0]=idx1; curr[1]=idx2; curr[2]=MINIMUM_INIT; curr[3]=MAXIMUM_INIT;
  for (int j=curr[0]; j<=curr[1]; j++) {
    curr[2] = least(curr[2],_tSpan[j][0]);
    curr[3] =  most(curr[3],_tSpan[j][1]);
  }
  int next[4];
  next[0]=curr[0]; next[1]=curr[1];
  for (int i=curr[2]; i<=curr[3]; i++) {
    next[0] = least(next[0],_sSpan[i][0]);
    next[1] =  most(next[1],_sSpan[i][1]);
  } 
  next[2] = curr[2]; next[3]= curr[3];
  int idx=1;
  do {
    // update the current
    for (int j=next[0]; j<curr[0]; j++) {
      curr[2] = least(curr[2],_tSpan[j][0]);
      curr[3] =  most(curr[3],_tSpan[j][1]);
    }
    for (int j=curr[1]+1; j<=next[1]; j++) {
      curr[2] = least(curr[2],_tSpan[j][0]);
      curr[3] =  most(curr[3],_tSpan[j][1]);
    }
    curr[0] = next[0]; curr[1] = next[1]; 
    if (curr[2]==next[2] && curr[3]==next[3]) break;
    // prepare for the next 
    for (int i=curr[2]; i<next[2]; i++) {
      next[0]= least(next[0],_sSpan[i][0]);
      next[1]=  most(next[1],_sSpan[i][1]);
    }
    for (int i=next[3]+1; i<=curr[3]; i++) {
      next[0] = least(next[0],_sSpan[i][0]);
      next[1] =  most(next[1],_sSpan[i][1]);
    }
    next[2] = curr[2]; next[3]= curr[3];
    idx++;
  } while(1);
  return curr;
}

int* Alignment::blockTarget(int idx1, int idx2) {
// outputs a minimal [s1,s2,t1,t2] that contains idx1 and idx2, where idx1<=idx2
  int *curr = new int[4];
  curr[0]=MINIMUM_INIT; curr[1]=MAXIMUM_INIT; curr[2]=idx1; curr[3]=idx2;
        for (int i=curr[2]; i<=curr[3]; i++) {
                curr[0] = least(curr[0],_sSpan[i][0]);
                curr[1] =  most(curr[1],_sSpan[i][1]);
        }
        int next[4];
        next[2]=curr[2]; next[3]=curr[3];
        for (int j=curr[0]; j<=curr[1]; j++) {
                next[2] = least(next[2],_tSpan[j][0]);
                next[3] =  most(next[3],_tSpan[j][1]);
        }
        next[0] = curr[0]; next[1]= curr[1];
        int idx=1;
        do {
                // update the current
                for (int i=next[2]; i<curr[2]; i++) {
                        curr[0] = least(curr[0],_sSpan[i][0]);
                        curr[1] =  most(curr[1],_sSpan[i][1]);
                }
                for (int i=curr[3]+1; i<=next[3]; i++) {
                        curr[0] = least(curr[0],_sSpan[i][0]);
                        curr[1] =  most(curr[1],_sSpan[i][1]);
                }
                curr[2] = next[2]; curr[3] = next[3];
                if (curr[0]==next[0] && curr[1]==next[1]) break;
                // prepare for the next
                for (int j=curr[0]; j<next[0]; j++) {
                        next[2]= least(next[2],_tSpan[j][0]);
                        next[3]=  most(next[3],_tSpan[j][1]);
                }
                for (int j=next[1]+1; j<=curr[1]; j++) {
                        next[2] = least(next[2],_tSpan[j][0]);
                        next[3] =  most(next[3],_tSpan[j][1]);
                }
                next[0] = curr[0]; next[1]= curr[1];
                idx++;
        } while(1);
  return curr;
}

int Alignment::firstSourceAligned(int start) {
  for (int j=start; j<_J; j++) 
    if (_tSpan[j][0]!=MINIMUM_INIT) return j;
  return -1;
}

int Alignment::lastSourceAligned(int end) {
  for (int j=end; j>=0; j--)
    if (_tSpan[j][0]!=MINIMUM_INIT) return j;
  return -1;
}

int Alignment::firstTargetAligned(int start) {
  for (int i=start; i<_I; i++) 
    if (_sSpan[i][0]!=MINIMUM_INIT) return i;
  return -1;
}

int Alignment::lastTargetAligned(int end) {
  for (int i=end; i>=0; i--) 
    if (_sSpan[i][0]!=MINIMUM_INIT) return i;
  return -1;
}

void Alignment::BorderingSFWsOnly() {
// removes the record of all function word alignments, except those at the borders
// the number of alignments kept may be more than two
// i.e. where the leftmost / the rightmost alignments are unaligned. 
// In such cases, this function continues keeping function word alignments until the 
// first (or last) alignment words. 
  if (SourceFWIdxs[0]>2) {
    int firstCut = 1;
    for (int j=2; j<=SourceFWIdxs[0]; j++) {
      if (SourceFWIdxs[3*j-2]>fas) break;
      firstCut=j;
    }
    int lastCut  = SourceFWIdxs[0];
    for (int j=SourceFWIdxs[0]-1; j>=0; j--) {
      if (SourceFWIdxs[3*j-2]<las) break;
      lastCut=j;
    }
    if (firstCut>=lastCut) return;
    int delta = 0;
    for (int j=lastCut; j<=SourceFWIdxs[0]; j++) {
      delta++;
      SourceFWIdxs[3*(firstCut+delta)-2]=SourceFWIdxs[3*j-2];
      SourceFWIdxs[3*(firstCut+delta)-1]=SourceFWIdxs[3*j-1];
      SourceFWIdxs[3*(firstCut+delta)]  =SourceFWIdxs[3*j];
    }
    SourceFWIdxs[0]=firstCut+delta;
  }
}

void Alignment::BorderingTFWsOnly() {
// similar to BorderingSFWsOnly() except this looks at the source side.
  if (TargetFWIdxs[0]>2) {
    int firstCut = 1;
    for (int j=2; j<=TargetFWIdxs[0]; j++) {
      if (TargetFWIdxs[3*j-2]>fat) break;
      firstCut=j;
    }
    int lastCut  = TargetFWIdxs[0];
    for (int j=TargetFWIdxs[0]-1; j>=0; j--) {
      if (TargetFWIdxs[3*j-2]<lat) break;
      lastCut=j;
    }
    if (firstCut>=lastCut) return;
    int delta = 0;
    for (int j=lastCut; j<=TargetFWIdxs[0]; j++) {
      delta++;
      TargetFWIdxs[3*(firstCut+delta)-2]=TargetFWIdxs[3*j-2];
      TargetFWIdxs[3*(firstCut+delta)-1]=TargetFWIdxs[3*j-1];
      TargetFWIdxs[3*(firstCut+delta)]  =TargetFWIdxs[3*j];
    }
    TargetFWIdxs[0]=firstCut+delta;
  }
}

void Alignment::FillFWIdxsState(int* state, int fas, int las, int fat, int lat) {
  if (DEBUG) cerr << "FillFWIdxsState ("<< fas <<","<< las<<"," << fat <<"," << lat << ")" << endl;
  if (fas==las) las+=1;
  if (fat==lat) lat+=1;
  for (int idx=0; idx<12; idx++) state[idx]=-1;
  if (SourceFWIdxs[0]<=2) {
    if (SourceFWIdxs[0]>=1) {state[0]=SourceFWIdxs[1]; state[1]=SourceFWIdxs[2]; state[2]=SourceFWIdxs[3];}
    if (SourceFWIdxs[0]==2) {state[3]=SourceFWIdxs[4]; state[4]=SourceFWIdxs[5]; state[5]=SourceFWIdxs[6];}
  } else {
    if (SourceFWIdxs[1]>fas) {
      state[0]=SourceFWIdxs[1]; state[1]=SourceFWIdxs[2]; state[2]=SourceFWIdxs[3];
    } else {
      ostringstream issf; ostringstream isse;
      for (int idx=1; idx<=SourceFWIdxs[0]; idx++) {
        if (SourceFWIdxs[3*idx-2]>las) break;
        if (idx>1) { issf << " "; isse << " ";};
        issf << TD::Convert(SourceFWIdxs[3*idx-1]);
        isse << TD::Convert(SourceFWIdxs[3*idx]);
        state[0]=SourceFWIdxs[3*idx-2];
        if (state[0]>=fas) break;
      }
      if (state[0]>=0) {
        state[1]=TD::Convert(issf.str())*-1; state[2]=TD::Convert(isse.str()); //multiplying source with -1 as marker
      }
    }
    if (SourceFWIdxs[SourceFWIdxs[0]*3-2]==las) {
      state[3]=SourceFWIdxs[SourceFWIdxs[0]*3-2];
      state[4]=SourceFWIdxs[SourceFWIdxs[0]*3-1]; 
      state[5]=SourceFWIdxs[SourceFWIdxs[0]*3];
    } else {
      int lastCut = SourceFWIdxs[0];
      for (int j=lastCut-1; j>=state[0]+1; j--) {
        if (SourceFWIdxs[3*j-2]==state[0]) break;
        if (SourceFWIdxs[3*j-2]<las) break;
        lastCut=j;
      }
      state[3]=SourceFWIdxs[3*lastCut-2];
      ostringstream issf; ostringstream isse;
      for (int idx=lastCut; idx<=SourceFWIdxs[0]; idx++) {
        if (idx>lastCut) { issf << " "; isse << " ";};
        issf << TD::Convert(SourceFWIdxs[3*idx-1]);
        isse << TD::Convert(SourceFWIdxs[3*idx]);
      }
      if (state[3]>=0) {
        //multiplying source with -1 as compound marker
        state[4]=TD::Convert(issf.str())*-1; state[5]=TD::Convert(isse.str()); 
      }
    }
  }
  if (TargetFWIdxs[0]<=2) {
    if (TargetFWIdxs[0]>=1) {state[6]=TargetFWIdxs[1]; state[7]=TargetFWIdxs[2]; state[8]=TargetFWIdxs[3];}
    if (TargetFWIdxs[0]==2) {state[9]=TargetFWIdxs[4]; state[10]=TargetFWIdxs[5]; state[11]=TargetFWIdxs[6];}
  } else {
    if (TargetFWIdxs[1]>fat) { //shouldn't come here if SetTargetBorderingFW is invoked
      state[6]=TargetFWIdxs[1]; state[7]=TargetFWIdxs[2]; state[8]=TargetFWIdxs[3];
    } else {
      ostringstream issf; ostringstream isse;
      for (int idx=1; idx<=TargetFWIdxs[0]; idx++) {
        if (TargetFWIdxs[3*idx-2]>fat) break;
        if (idx>1) { issf << " "; isse << " ";};
        issf << TD::Convert(TargetFWIdxs[3*idx-1]);
        isse << TD::Convert(TargetFWIdxs[3*idx]);
        state[6]=TargetFWIdxs[3*idx-2];
      }
      state[7]=TD::Convert(issf.str()); state[8]=TD::Convert(isse.str())*-1; 
      //multiplying target with -1 as compound marker
    }
    if (TargetFWIdxs[TargetFWIdxs[0]*3-2]==lat) {
      state[9]=TargetFWIdxs[TargetFWIdxs[0]*3-2];
      state[10]=TargetFWIdxs[TargetFWIdxs[0]*3-1];
      state[11]=TargetFWIdxs[TargetFWIdxs[0]*3];
    } else {
      int lastCut = TargetFWIdxs[0];
      for (int j=lastCut-1; j>=1; j--) {
        if (TargetFWIdxs[3*j-2]<=state[9]) break;
        if (TargetFWIdxs[3*j-2]<lat) break;
        lastCut=j;
      }
      state[9]=TargetFWIdxs[3*lastCut-2];
      ostringstream issf; ostringstream isse;
      for (int idx=lastCut; idx<=TargetFWIdxs[0]; idx++) {
        if (idx>lastCut) issf << " "; isse << " ";;
        issf << TD::Convert(TargetFWIdxs[3*idx-1]);
        isse << TD::Convert(TargetFWIdxs[3*idx]);
      }
      state[10]=TD::Convert(issf.str()); state[11]=TD::Convert(isse.str())*-1; 
    }
  }
}

void Alignment::simplifyBackward(vector<int *>*blocks, int* block, const vector<int>& danglings) {
// given a *block*, see whether its target span contains any index inside *danglings*. 
// if yes, break it; otherwise, keep it. put the result(s) to *blocks*
  if (DEBUG) cerr << "simplifyBackward[" << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << "]" << endl;
  if (DEBUG) for (int i=0; i<danglings.size(); i++) cerr << "danglings[" << i << "] = " << danglings[i] << endl;
  if (danglings.size()==0) {
    blocks->push_back(block); 
    if (DEBUG) cerr << "pushing(0) " << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
    return; 
  }
  int currIdx = block[2];
  int i_dangling = 0;
  while (block[2]>danglings[i_dangling])  {
    if (i_dangling+1 >= danglings.size()) break;
    i_dangling++;
  }
  while (danglings[i_dangling]==currIdx) {
    i_dangling++;
    currIdx++;
  } 
  /*if (i_dangling>=danglings.size() && currIdx) {
    blocks->push_back(block); 
    if (DEBUG) cerr << "pushing(1) " << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
    return;
  }
  if (block[3]<danglings[i_dangling]) {
    blocks->push_back(block); 
    if (DEBUG) cerr << "pushing(2) " << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
    return;
  }*/
  if (DEBUG) cerr << "i_dangling = " << i_dangling << endl;
  int anchorIdx = danglings[i_dangling];
  if (i_dangling+1>=danglings.size() || anchorIdx>block[3]+1) anchorIdx=block[3]+1;
  if (DEBUG) cerr << "anchorIdx = " << anchorIdx << ", currIdx = " << currIdx << endl;
  do {
    while(currIdx<anchorIdx) {
      if (DEBUG) cerr << "currIdx = " << currIdx << ", anchorIdx = " << anchorIdx << endl;
      bool isMoved = false;
      for (int idx=anchorIdx-1; idx>=currIdx; idx--) {
        int *nublock = blockTarget(currIdx,idx);
        if (nublock[2]==currIdx && nublock[3]==idx) {
          if (nublock[0]!=MINIMUM_INIT) {
            blocks->push_back(nublock);
            if (DEBUG) cerr << "pushing(3) " << nublock[0] << "," << nublock[1] << "," << nublock[2] << "," << nublock[3] << endl;
          } else { 
            delete nublock; 
          }
          isMoved = true;
          currIdx=idx+1; break;
        } else {
          delete nublock;
        }
      }
      if (DEBUG) cerr << "isMoved=" << isMoved << ", currIdx=" << currIdx << endl;
      if (!isMoved) {
        int source = sourceOf(currIdx);
        while (source>=0) {
          if (source >= block[0]) {
            int* nublock = new int[4];
            nublock[0]=source; nublock[1]=source; nublock[2]=currIdx; nublock[3]=currIdx;
            blocks->push_back(nublock);
            if (DEBUG) cerr << "pushing(4) " << nublock[0] << "," << nublock[1] << "," << nublock[2] << "," << nublock[3] << endl;
          } 
          source = sourceOf(currIdx,source+1);
        }
        currIdx++;
      }
    }
    currIdx=anchorIdx+1;
    anchorIdx=block[3]+1;
    if (i_dangling+1<danglings.size()) anchorIdx=danglings[++i_dangling];
  } while(currIdx<=block[3]);
}

void Alignment::simplify(int* ret) {
  // the idea is to create blocks of maximal consistent alignment in between a pair of function words
  // exceptional cases include: one to non-contiguous many (or vice versa) -> treat this as one alignment each
  // record all function word alignments first, important because it may be unaligned
  // return true if it's truly simple (no function word alignment involves); false, otherwise
  if (DEBUG) cerr << "begin simplify" << endl;
  reset(0,0); reset(_J-1,_I-1); // remove the phrase boundary alignments, NEED TO CHECK AGAIN !!!
  if (SourceFWIdxs[0]+TargetFWIdxs[0]==0) { // return singleton
    if (DEBUG) cerr << "no function words" << endl;
    for (int idx=0; idx<12; idx++) ret[idx]=-1;
    ret[12]=1; ret[13]=0; ret[14]=0; // 0-0
    FillFWIdxsState(ret,0,0,0,0);  
    return;
  }
  curr_al.insert(curr_al.begin(),curr_al.size());
  curr_al.push_back(SourceFWIdxs[0]);
  for (int i=1; i<=SourceFWIdxs[0]; i++) curr_al.push_back(SourceFWIdxs[3*i-2]);
  curr_al.push_back(TargetFWIdxs[0]);
  for (int i=1; i<=TargetFWIdxs[0]; i++) curr_al.push_back(TargetFWIdxs[3*i-2]);
  vector<int> el;
  if (simplify_hash.find(curr_al)==simplify_hash.end()) {
    if (DEBUG) {
      cerr << "SourceFWIdxs:" << SourceFWIdxs[0] << endl;
      for (int i=1; i<=SourceFWIdxs[0]; i++)  
        cerr << SourceFWIdxs[3*i-2] << "," <<  SourceFWIdxs[3*i-1] << "," << SourceFWIdxs[3*i] << endl;  
      cerr << "TargetFWIdxs:" << TargetFWIdxs[0] << endl;
      for (int i=1; i<=TargetFWIdxs[0]; i++) { 
        cerr << TargetFWIdxs[3*i-2] << "," <<  TargetFWIdxs[3*i-1] << "," << TargetFWIdxs[3*i] << endl;  
      }
    }

    vector< int* > blocks; // each element contains s1,s2,t1,t2
    int currIdx = 1; // start from 1 to avoid considering phrase start
    std::set<int> FWIdxs;
    std::vector<int> DanglingTargetFWIdxs;
    for (int i=1; i<= SourceFWIdxs[0]; i++) FWIdxs.insert(SourceFWIdxs[3*i-2]);
    for (int i=1; i<= TargetFWIdxs[0]; i++) {
      int source = sourceOf(TargetFWIdxs[3*i-2]);
      if (source>=0) {
        do {
          FWIdxs.insert(source);
          source = sourceOf(TargetFWIdxs[3*i-2],source+1);
        } while(source >=0);
      } else {
        int *block = new int[4];
        block[0]=-1; block[1]=-1; block[2]=TargetFWIdxs[3*i-2]; block[3]=TargetFWIdxs[3*i-2];
        blocks.push_back(block);
        if (DEBUG) cerr << "pushing[1] " << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
        DanglingTargetFWIdxs.push_back(TargetFWIdxs[3*i-2]);  
      }
    }
    if (DEBUG)
      for (std::set<int>::const_iterator iter=FWIdxs.begin(); iter!=FWIdxs.end(); iter++) { 
        cerr << "FWIdxs=" << *iter << endl;
      }
    std::set<int>::const_iterator currFWIdx  = FWIdxs.begin();
    if (currFWIdx == FWIdxs.end()) {
      int* block = new int[4];
      block[0]=1; block[1]=_J-2; block[2]=1; block[3]=_I-2; // no need to consider phrase boundaries
      simplifyBackward(&blocks,block,DanglingTargetFWIdxs);
    } else {
      int anchorIdx = *currFWIdx; // also used to denote _J+1
      do {
        // add alignments whose source from currIdx to currFWIdx-1
        while (currIdx<anchorIdx) {
          bool isMoved = false;
          //cerr << "anchorIdx = " << anchorIdx << ", currIdx = " << currIdx << endl;
          for (int idx=anchorIdx-1; idx>=currIdx; idx--) {
            int* block = blockSource(currIdx,idx);
            if (block[0]==currIdx&&block[1]==idx) {
              if (block[2]!=MINIMUM_INIT) { // must be aligned
                simplifyBackward(&blocks,block,DanglingTargetFWIdxs); 
              } else {
                delete block;
              }
              currIdx = idx+1; isMoved = true;
              break;
            } else {
              delete block;
            }
          }
          if (!isMoved) {
            int target = targetOf(currIdx);
            while (target>=0) {
              int* block = new int[4];
              block[0]=currIdx; block[1]=currIdx; block[2]=target; block[3]=target;
              blocks.push_back(block);
              if (DEBUG) cerr << "pushing[2] " << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
              target = targetOf(currIdx,target+1);
            }
            currIdx++;
          }
        }    
        // add function word alignments (anchorIdx)
        if (anchorIdx==getJ()) break;
        int target = targetOf(anchorIdx);
        do {
          int* block = new int[4];
          block[0]=anchorIdx; block[1]=anchorIdx; block[2]=target; block[3]=target;
          blocks.push_back(block);
          if (DEBUG) cerr << "pushing[3] " << block[0] << "," << block[1] << "," << block[2] << "," << block[3] << endl;
          if (target>=0) target = targetOf(anchorIdx,target+1);
        } while (target>=0);
        // advance indexes
        currIdx   = anchorIdx+1;
        anchorIdx = getJ()-1; // was minus 2
        if (++currFWIdx!=FWIdxs.end()) anchorIdx = *currFWIdx;
      } while (currIdx<=getJ()-2);
    }  

    
    vector<int> source_block_mapper(getJ(),-1);
    vector<int> target_block_mapper(getI(),-1);
    for (int i = 0; i<blocks.size(); i++) {
      if (DEBUG) cerr << "blocks[" << i << "]=" << blocks[i][0] << "," << blocks[i][1] << "," << blocks[i][2] << "," << blocks[i][3] << endl;
      if (blocks[i][0]>=0) source_block_mapper[blocks[i][0]]=1;
      if (blocks[i][2]>=0) target_block_mapper[blocks[i][2]]=1; 
    }
    int curr = 1;
    int prev = -1;
    for (int idx=0; idx<source_block_mapper.size(); idx++) {
      if (source_block_mapper[idx]>0) {
        source_block_mapper[idx]=curr++;
        prev = curr;
      } else {
        source_block_mapper[idx]=prev;
      }
    }
    curr = 1;
    for (int idx=0; idx<target_block_mapper.size(); idx++) {
      if (target_block_mapper[idx]>0) {
        target_block_mapper[idx]=curr++;
        prev = curr;
      } else {
        target_block_mapper[idx]=prev;
      }
    }
    
    //assert(blocks.size()<=50);
    if (DEBUG) cerr << "resulting alignment:" << endl;
    for (int i = 0; i<blocks.size(); i++) {
      if (blocks[i][2]<0 || blocks[i][0]<0) continue;
      int source = source_block_mapper[blocks[i][0]]-1;
      int target = target_block_mapper[blocks[i][2]]-1;
      el.push_back(link(source,target));
      if (DEBUG) cerr << source << "-" << target << " ";
    }
    el.insert(el.begin(),el.size());
    if (DEBUG) cerr << endl;
    el.push_back(SourceFWIdxs[0]);
    for (int idx=1; idx<=SourceFWIdxs[0]; idx++) {
      if (DEBUG) cerr << "SourceFWIdxs[" << (3*idx-2) << "] from " << SourceFWIdxs[3*idx-2] << endl;
      el.push_back(source_block_mapper[SourceFWIdxs[3*idx-2]]-1);
    }
    el.push_back(TargetFWIdxs[0]);
    for (int idx=1; idx<=TargetFWIdxs[0]; idx++) {
      if (DEBUG) cerr << "TargetFWIdxs[" << (3*idx-2) << "] from " << TargetFWIdxs[3*idx-2] << endl;
      el.push_back(target_block_mapper[TargetFWIdxs[3*idx-2]]-1);
    }
    el.push_back(source_block_mapper[fas]-1); 
    el.push_back(source_block_mapper[las]-1);
    el.push_back(target_block_mapper[fat]-1);
    el.push_back(target_block_mapper[lat]-1);
    if (DEBUG) {
      cerr << "insert key:el = ";
      for (int ii=0; ii<el.size(); ii++) 
        cerr << ii << "." << el[ii] << " ";
      cerr << " || " << endl;
    }
    if (DEBUG) cerr << "trying to insertL " <<  endl;
    if (DEBUG) {
      cerr << "size=" << curr_al.size() << " ";
      for (int ii=0; ii<curr_al.size(); ii++) cerr << "curr_al[" << ii << "]=" << curr_al[ii] << " ";
      cerr << endl;
    }
    simplify_hash.insert(pair<vector<int>, vector<int> > (curr_al,el));
    if (DEBUG) cerr << "inserted" << endl;
  } else {
    el = simplify_hash[curr_al];
  }
  if (DEBUG) {
    cerr << "pull key:el = ";
    for (int ii=0; ii<el.size(); ii++)
      cerr << ii << "." << el[ii] << " ";
    cerr << endl;
  }
  ret[12] = el[0];
  for (int i=1; i<=el[0]; i++) ret[12+i] = el[i];
  int istart = el[0]+1;
  assert(el[istart]==SourceFWIdxs[0]);
  for (int i=1; i<=el[istart]; i++) SourceFWIdxs[3*i-2]=el[istart+i];
  istart += el[istart]+1;
  assert(el[istart]==TargetFWIdxs[0]);
  for (int i=1; i<=el[istart]; i++) TargetFWIdxs[3*i-2]=el[istart+i];
  istart += el[istart]+1;
  FillFWIdxsState(ret,el[istart],el[istart+1],el[istart+2],el[istart+3]);
}

void Alignment::simplify_nofw(int* ret) {
  for (int i=0; i<12; i++) ret[i]=-1;  
  ret[12]=1; ret[13]=0; 
}

void Alignment::sort(int* num) {
  if (num[0]>1) quickSort(num,1,num[0]);
}

void Alignment::quickSort(int arr[], int left, int right) {
  int i = left, j = right;
  int tmp1,tmp2,tmp3;
  int mid = (left + right) / 2;
  int pivot = arr[3*mid-2];
 
  /* partition */
  while (i <= j) {
    while (arr[3*i-2] < pivot) i++;
    while (arr[3*j-2] > pivot) j--;
    if (i <= j) {
      tmp1 = arr[3*i-2]; tmp2 = arr[3*i-1]; tmp3 = arr[3*i];
      arr[3*i-2] = arr[3*j-2]; arr[3*i-1] = arr[3*j-1]; arr[3*i] = arr[3*j];
      arr[3*j-2] = tmp1; arr[3*j-1] = tmp2; arr[3*j] = tmp3;
      i++;
      j--;
    }
  };
 
  /* recursion */
  if (left < j) quickSort(arr, left, j);
  if (i < right) quickSort(arr, i, right);
}

double Alignment::ScoreOrientation(const CountTable& table, int offset, int ori, WordID cond1, WordID cond2) {
  string source = TD::Convert(cond1);
  string sourceidx;
  if (table.mode == 1) {
    sourceidx = source; 
    int slashidx = sourceidx.find_last_of("/");
    source = sourceidx.substr(0,slashidx);
    string idx = sourceidx.substr(slashidx+1);
    if (DEBUG) cerr << "   sourceidx = " << sourceidx << ", idx = " << idx << endl;
    if (idx == "X") {
      if (DEBUG) cerr << "        idx == X, returning 0" << endl;
      return 0;
    }
  }
  string target = TD::Convert(cond2);
  if (DEBUG) cerr << "sourceidx='" << sourceidx << "', source='" << source << "', target='" << target << "'" << endl;
  double count = table.ultimate[offset+ori-1];
  double total = table.ultimate[offset+5];   
  double alpha = 0.1;
  double prob = count/total;
  if (DEBUG) cerr << "level0 " << count << "/" << total << "=" << prob << endl;
  
  WordID key_id = (table.mode!=1) ? cond1 : TD::Convert(source); 
  map<WordID,int*>::const_iterator it = table.model.find(key_id);
  bool stop = (it==table.model.end());
  if (!stop) {
    stop=true;
    if (it->second[offset+5]>=0) {
      count = it->second[offset+ori-1] + alpha * prob;
      total = it->second[offset+5] + alpha;
      prob = count/total;
      stop = false;
      if (DEBUG) cerr << "level1 " << count << "/" << total << "=" << prob << endl;
    }
  }
  if (stop) return prob;

  string key = source + " " + target;
  it = table.model.find(TD::Convert(key));
  stop = (it==table.model.end());
  if (!stop) {
    stop = true;
    if (it->second[offset+5]>=0) {
      count = it->second[offset+ori-1] + alpha * prob;
      total = it->second[offset+5] + alpha;
      prob = count/total;
      stop = false;
      if (DEBUG) cerr << "level2 " << count << "/" << total << "=" << prob << endl;
    }
  }

  if (stop || table.mode!=1) return prob;
  
  key = sourceidx + " " + target;
  it = table.model.find(TD::Convert(key));
  if (it!=table.model.end()) {
    if (it->second[offset+5]>=0) {
      count = it->second[offset+ori-1] + alpha * prob;
      total = it->second[offset+5] + alpha;
      prob = count/total;
      if (DEBUG) cerr << "level3 " << count << "/" << total << "=" << prob << endl;
    }
  }

  return prob;
}
 
void Alignment::ScoreOrientation(const CountTable& table, int offset, int ori, WordID cond1, WordID cond2, 
  bool isBonus, double *cost, double *bonus, double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus,
  double alpha1, double beta1) {
  if (DEBUG) cerr << "ScoreOrientation:" << TD::Convert(cond1) << "," << TD::Convert(cond2) << ", alpha1 = " << alpha1 << ", beta1 = " << beta1 << endl;
  double ret = ScoreOrientation(table,offset,ori,cond1,cond2);
  if (isBonus) {
    if (table.mode == 0) *bonus += log(ret); else *bonus += ret;
  } else {
    if (table.mode == 0) *cost += log(ret); else *cost += ret;
  }
}

double Alignment::ScoreOrientationLeft(const CountTable& table, int ori, WordID cond1, WordID cond2) {
  double ret = ScoreOrientation(table,0,ori,cond1,cond2); 
  if (table.mode == 0) return log(ret);
  return ret;
}

double Alignment::ScoreOrientationLeftBackward(const CountTable& table, int ori, WordID cond1, WordID cond2) {
  double ret = ScoreOrientation(table,12,ori,cond1,cond2); 
  if (table.mode == 0) return log(ret);
  return ret;
}
 
double Alignment::ScoreOrientationRight(const CountTable& table, int ori, WordID cond1, WordID cond2) {
  double ret = ScoreOrientation(table,6,ori,cond1,cond2); 
  if (table.mode == 0) return log(ret);
  return ret;
}

double Alignment::ScoreOrientationRightBackward(const CountTable& table, int ori, WordID cond1, WordID cond2) {
  double ret = ScoreOrientation(table,18,ori,cond1,cond2); 
  if (table.mode == 0) return log(ret);
  return ret;
}

void Alignment::ScoreOrientationLeft(const CountTable& table, int ori, WordID cond1, WordID cond2, 
  bool isBonus, double *cost, double *bonus, double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus, double alpha1, double beta1) {
  if (DEBUG) cerr << "ScoreOrientationLeft(" << isBonus << ")" << endl;
  ScoreOrientation(table,0,ori,cond1,cond2,isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha1,beta1);
}

void Alignment::ScoreOrientationLeftBackward(const CountTable& table, int ori, WordID cond1, WordID cond2, 
  bool isBonus, double *cost, double *bonus, double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus, double alpha1, double beta1) {
  if (DEBUG) cerr << "ScoreOrientationLeftBackward" << endl;
  ScoreOrientation(table,12,ori,cond1,cond2,isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha1,beta1);
}

void Alignment::ScoreOrientationRight(const CountTable& table, int ori, WordID cond1, WordID cond2, 
  bool isBonus, double *cost, double *bonus, double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus, double alpha1, double beta1) {
  if (DEBUG) cerr << "ScoreOrientationRight(" << isBonus << ")" << endl;
  ScoreOrientation(table,6,ori,cond1,cond2,isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha1,beta1);
}

void Alignment::ScoreOrientationRightBackward(const CountTable& table, int ori, WordID cond1, WordID cond2,
  bool isBonus, double *cost, double *bonus, double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus, double alpha1, double beta1) {
  if (DEBUG) cerr << "ScoreOrientationRightBackward" << endl;
  ScoreOrientation(table,18,ori,cond1,cond2,isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha1,beta1);
}

void Alignment::computeOrientationSourceBackwardPos(const CountTable& table, double *cost, double *bonus,
                                                 double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus, int maxfwidx, int maxdepth1, int maxdepth2) {
  if (DEBUG) cerr << "computeOrientationSourceBackward" << endl;
  int oril, orir;
  for (int idx=1; idx<=SourceFWRuleIdxs[0]; idx++) {
    if (DEBUG) cerr << "considering SourceFWRuleIdxs[" << idx << "]: " << SourceFWRuleIdxs[3*idx-2] << endl;
    if (!(SourceFWRuleAbsIdxs[idx]<=maxdepth1 || maxfwidx-SourceFWRuleAbsIdxs[idx]+1<=maxdepth2)) continue;
    int* fwblock = blockSource(SourceFWRuleIdxs[3*idx-2],SourceFWRuleIdxs[3*idx-2]);
    bool aligned = (fwblock[2]!=MINIMUM_INIT);
    if (aligned) {
      OrientationTarget(fwblock[2],fwblock[3],&oril,&orir);
    } else {
      OrientationSource(SourceFWRuleIdxs[3*idx-2],&oril,&orir);
    }
    if (DEBUG) cerr << "oril = " << oril << ", orir = " << orir << endl;
    bool isBonus = false; // fas -> first aligned source word, las -> last aligned source word
    if ((aligned && fwblock[2]<=fat)||
        (!aligned && SourceFWRuleIdxs[3*idx-2]<=fas)) isBonus=true;
    if (SourceFWRuleAbsIdxs[idx]<=maxdepth1) {
      ostringstream nusource;
      nusource << TD::Convert(SourceFWRuleIdxs[3*idx-1]) << "/" << SourceFWRuleAbsIdxs[idx];
      ScoreOrientationLeftBackward(table,oril,TD::Convert(nusource.str()),SourceFWRuleIdxs[3*idx],
                         isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
    }
    if (maxfwidx-SourceFWRuleAbsIdxs[idx]+1<=maxdepth2) {
      ostringstream nusource;
      nusource << TD::Convert(SourceFWRuleIdxs[3*idx-1]) << "/" << ((maxfwidx-SourceFWRuleAbsIdxs[idx]+1)*-1);
      ScoreOrientationLeftBackward(table,oril,TD::Convert(nusource.str()),SourceFWRuleIdxs[3*idx],
                         isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
    }
    isBonus = false;
    if ((aligned && lat<=fwblock[3])||
        (!aligned && las<=SourceFWRuleIdxs[3*idx-2])) isBonus=true;
    if (SourceFWRuleAbsIdxs[idx]<=maxdepth1) {
      ostringstream nusource;
      nusource << TD::Convert(SourceFWRuleIdxs[3*idx-1]) << "/" << SourceFWRuleAbsIdxs[idx];
      ScoreOrientationRightBackward(table,orir,SourceFWRuleIdxs[3*idx-1],SourceFWRuleIdxs[3*idx],
                          isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
    }
    if (maxfwidx-SourceFWRuleAbsIdxs[idx]+1<=maxdepth2) {
      ostringstream nusource;
      nusource << TD::Convert(SourceFWRuleIdxs[3*idx-1]) << "/" << ((maxfwidx-SourceFWRuleAbsIdxs[idx]+1)*-1);
      ScoreOrientationRightBackward(table,orir,SourceFWRuleIdxs[3*idx-1],SourceFWRuleIdxs[3*idx],
                          isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
    }
    delete fwblock;
  }
  for (int i_ant=0; i_ant<_Arity; i_ant++) {
    // antfas -> first aligned source word antecedent-wise
    // antlas -> last aligned source word antecedent-wise
    int antfat = firstTargetAligned(TargetAntsIdxs[i_ant][1]);
    int antlat = lastTargetAligned(TargetAntsIdxs[i_ant][TargetAntsIdxs[i_ant][0]]);
    int antfas = firstSourceAligned(SourceAntsIdxs[i_ant][1]);
    int antlas = lastSourceAligned(SourceAntsIdxs[i_ant][SourceAntsIdxs[i_ant][0]]);
    assert(antfat <= antlat);
    assert(antfas <= antlas);
    for (int idx=1; idx<=SourceFWAntsIdxs[i_ant][0]; idx++) {
      if (DEBUG)
        cerr << "considering SourceFWAntsIdxs[" << i_ant << "][" << idx << "]: " << SourceFWAntsIdxs[i_ant][3*idx-2] << endl;
      if (!(SourceFWAntsAbsIdxs[i_ant][idx]<=maxdepth1 || maxfwidx-SourceFWAntsAbsIdxs[i_ant][idx]+1<=maxdepth2)) continue;
      int* fwblock = blockSource(SourceFWAntsIdxs[i_ant][3*idx-2],SourceFWAntsIdxs[i_ant][3*idx-2]);
      //bool aligned = (minTSpan(SourceFWAntsIdxs[i_ant][3*idx-2])!=MINIMUM_INIT);
      bool aligned = (fwblock[2]!=MINIMUM_INIT);
      bool Lcompute = true; bool Rcompute = true;
      if (DEBUG) {
        cerr << " aligned = " << aligned << endl;
        cerr << "  fwblock = " << fwblock[0] << "," << fwblock[1] << "," << fwblock[2] << "," << fwblock[3] << endl;
        cerr << "   antfas=" << antfas << ", antlas=" << antlas << ", antfat=" << antfat << ", antlat=" << antlat << endl;
      }
      if (aligned) {
        if (DEBUG) cerr << "laligned" << endl;
        if (antfat<fwblock[2]) {
          if (DEBUG) cerr << antfat << "<" << fwblock[2] << endl;
          Lcompute=false;
        }
      } else {
        if (DEBUG) cerr << "!laligned" << endl;
        if (antfas<fwblock[0] && fwblock[1] < antlas) Lcompute=false;
      }
      if (aligned) {
        if (DEBUG) cerr << "raligned" << endl;
        if (fwblock[3]<antlat) {
          if (DEBUG) cerr << fwblock[3] << "<" << antlat << endl;
          Rcompute=false;
        }
      } else {
        if (DEBUG) cerr << "!raligned" << endl;
        if (fwblock[1]<antlas && fwblock[1] < antlas) Rcompute=false;
      }
      if (!Lcompute && !Rcompute) continue;
      if (!aligned) {
        OrientationSource(SourceFWAntsIdxs[i_ant][3*idx-2],&oril,&orir,Lcompute,Rcompute);
      } else {
        OrientationTarget(fwblock[2],fwblock[3],&oril,&orir,Lcompute,Rcompute);
      }
      if (DEBUG) cerr << "oril = " << oril << ", orir = " << orir << endl;
      bool isBonus = false;
      if (Lcompute) {
        if ((aligned && fwblock[3]<=fat) ||
            (!aligned && SourceFWAntsIdxs[i_ant][3*idx-2]<=fas)) isBonus = true;
        if (SourceFWAntsAbsIdxs[i_ant][idx]<=maxdepth1) {
          ostringstream nusource;
          nusource << TD::Convert(SourceFWAntsIdxs[i_ant][3*idx-1]) << "/" << SourceFWAntsAbsIdxs[i_ant][idx];
          ScoreOrientationLeftBackward(table,oril,TD::Convert(nusource.str()),SourceFWAntsIdxs[i_ant][3*idx],
                             isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
        }
        if (maxfwidx-SourceFWAntsAbsIdxs[i_ant][idx]+1<=maxdepth2) {
          ostringstream nusource;
          nusource << TD::Convert(SourceFWAntsIdxs[i_ant][3*idx-1]) << "/" << (-1*(maxfwidx-SourceFWAntsAbsIdxs[i_ant][idx]+1));
          ScoreOrientationLeftBackward(table,oril,TD::Convert(nusource.str()),SourceFWAntsIdxs[i_ant][3*idx],
                             isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
        }
      }
      isBonus = false;
      if (Rcompute) {
        if ((aligned && lat<=fwblock[2]) ||
            (!aligned && las<=SourceFWAntsIdxs[i_ant][3*idx-2]))isBonus = true;
        if (SourceFWAntsAbsIdxs[i_ant][idx]<=maxdepth1) {
          ostringstream nusource;
          nusource << TD::Convert(SourceFWAntsIdxs[i_ant][3*idx-1]) << "/" << SourceFWAntsAbsIdxs[i_ant][idx];
          ScoreOrientationRightBackward(table,orir,TD::Convert(nusource.str()),SourceFWAntsIdxs[i_ant][3*idx],
                              isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
        }
        if (maxfwidx-SourceFWAntsAbsIdxs[i_ant][idx]+1<=maxdepth2) {
          ostringstream nusource;
          nusource << TD::Convert(SourceFWAntsIdxs[i_ant][3*idx-1]) << "/" << (-1*(maxfwidx-SourceFWAntsAbsIdxs[i_ant][idx]+1));
          ScoreOrientationRightBackward(table,orir,TD::Convert(nusource.str()),SourceFWAntsIdxs[i_ant][3*idx],
                              isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
        }
      }
      delete fwblock;
    }
  }
}


void Alignment::computeOrientationSourceBackward(const CountTable& table, double *cost, double *bonus, 
                                                 double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus) {
  if (DEBUG) cerr << "computeOrientationSourceBackward" << endl;
  int oril, orir;
  for (int idx=1; idx<=SourceFWRuleIdxs[0]; idx++) {
    if (DEBUG) cerr << "considering SourceFWRuleIdxs[" << idx << "]: " << SourceFWRuleIdxs[3*idx-2] << endl;
    int* fwblock = blockSource(SourceFWRuleIdxs[3*idx-2],SourceFWRuleIdxs[3*idx-2]);
    bool aligned = (fwblock[2]!=MINIMUM_INIT);
    if (aligned) {
      OrientationTarget(fwblock[2],fwblock[3],&oril,&orir);
    } else {
      OrientationSource(SourceFWRuleIdxs[3*idx-2],&oril,&orir);
    }
    if (DEBUG) cerr << "oril = " << oril << ", orir = " << orir << endl;
    bool isBonus = false; // fas -> first aligned source word, las -> last aligned source word
    if ((aligned && fwblock[2]<=fat)||
        (!aligned && SourceFWRuleIdxs[3*idx-2]<=fas)) isBonus=true;
    ScoreOrientationLeftBackward(table,oril,SourceFWRuleIdxs[3*idx-1],SourceFWRuleIdxs[3*idx],
                         isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
    isBonus = false;
    if ((aligned && lat<=fwblock[3])||
        (!aligned && las<=SourceFWRuleIdxs[3*idx-2])) isBonus=true;
    ScoreOrientationRightBackward(table,orir,SourceFWRuleIdxs[3*idx-1],SourceFWRuleIdxs[3*idx],
                          isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
    delete fwblock;
  }
  for (int i_ant=0; i_ant<_Arity; i_ant++) {
    // antfas -> first aligned source word antecedent-wise
    // antlas -> last aligned source word antecedent-wise
    int antfat = firstTargetAligned(TargetAntsIdxs[i_ant][1]);
    int antlat = lastTargetAligned(TargetAntsIdxs[i_ant][TargetAntsIdxs[i_ant][0]]);
    int antfas = firstSourceAligned(SourceAntsIdxs[i_ant][1]);
    int antlas = lastSourceAligned(SourceAntsIdxs[i_ant][SourceAntsIdxs[i_ant][0]]);
    assert(antfat <= antlat);
    assert(antfas <= antlas);
    for (int idx=1; idx<=SourceFWAntsIdxs[i_ant][0]; idx++) {
      if (DEBUG)
        cerr << "considering SourceFWAntsIdxs[" << i_ant << "][" << idx << "]: " << SourceFWAntsIdxs[i_ant][3*idx-2] << endl;
      int* fwblock = blockSource(SourceFWAntsIdxs[i_ant][3*idx-2],SourceFWAntsIdxs[i_ant][3*idx-2]);
      //bool aligned = (minTSpan(SourceFWAntsIdxs[i_ant][3*idx-2])!=MINIMUM_INIT);
      bool aligned = (fwblock[2]!=MINIMUM_INIT);
      bool Lcompute = true; bool Rcompute = true;
      if (DEBUG) {
        cerr << " aligned = " << aligned << endl;
        cerr << "  fwblock = " << fwblock[0] << "," << fwblock[1] << "," << fwblock[2] << "," << fwblock[3] << endl;
        cerr << "   antfas=" << antfas << ", antlas=" << antlas << ", antfat=" << antfat << ", antlat=" << antlat << endl; 
      }
      if (aligned) {
        if (DEBUG) cerr << "laligned" << endl;
        if (antfat<fwblock[2]) { 
          if (DEBUG) cerr << antfat << "<" << fwblock[2] << endl;
          Lcompute=false;
        }
      } else {
        if (DEBUG) cerr << "!laligned" << endl;
        if (antfas<fwblock[0] && fwblock[1] < antlas) Lcompute=false;
      }
      if (aligned) { 
        if (DEBUG) cerr << "raligned" << endl;
        if (fwblock[3]<antlat) { 
          if (DEBUG) cerr << fwblock[3] << "<" << antlat << endl;
          Rcompute=false;
        }
      } else {
        if (DEBUG) cerr << "!raligned" << endl;
        if (fwblock[1]<antlas && fwblock[1] < antlas) Rcompute=false;
      }
      if (!Lcompute && !Rcompute) continue;
      if (!aligned) {
        OrientationSource(SourceFWAntsIdxs[i_ant][3*idx-2],&oril,&orir,Lcompute,Rcompute);
      } else {
        OrientationTarget(fwblock[2],fwblock[3],&oril,&orir,Lcompute,Rcompute);
      }
      if (DEBUG) cerr << "oril = " << oril << ", orir = " << orir << endl;
      bool isBonus = false;
      if (Lcompute) {
        if ((aligned && fwblock[3]<=fat) ||
            (!aligned && SourceFWAntsIdxs[i_ant][3*idx-2]<=fas)) isBonus = true;
        ScoreOrientationLeftBackward(table,oril,SourceFWAntsIdxs[i_ant][3*idx-1],SourceFWAntsIdxs[i_ant][3*idx],
                             isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
      }
      isBonus = false;
      if (Rcompute) {
        if ((aligned && lat<=fwblock[2]) ||
            (!aligned && las<=SourceFWAntsIdxs[i_ant][3*idx-2]))isBonus = true;
        ScoreOrientationRightBackward(table,orir,SourceFWAntsIdxs[i_ant][3*idx-1],SourceFWAntsIdxs[i_ant][3*idx],
                              isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
      }
      delete fwblock;
    }
  }
}

void Alignment::computeOrientationSourcePos(const CountTable& table, double *cost, double *bonus,
                double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus, int maxfwidx, int maxdepth1, int maxdepth2) {
  // This implementation is actually really bad, not reusing codes at all
  if (DEBUG) cerr << "computeOrientationSourcePos(maxfwidx=" << maxfwidx << ",maxdepth=" << maxdepth1 << "," << maxdepth2 << ")" << endl;
  if (maxdepth1+maxdepth2==0) return;
  int oril, orir;
  ostringstream oss;
  WordID sourceID;
  for (int idx=1; idx<=SourceFWRuleIdxs[0]; idx++) {
    if (DEBUG) cerr << "considering SourceFWRuleIdxs[" << idx << "]: " << SourceFWRuleIdxs[3*idx-2] << endl;
    //if (!((SourceFWRuleAbsIdxs[idx]<=maxdepth1) || (maxfwidx-SourceFWRuleAbsIdxs[idx]+1<=maxdepth2))) continue; 
    string source = TD::Convert(SourceFWRuleIdxs[3*idx-1]);
    OrientationSource(SourceFWRuleIdxs[3*idx-2],&oril,&orir);
    bool isBonus = false; // fas -> first aligned source word, las -> last aligned source word
    if (SourceFWRuleIdxs[3*idx-2]<=fas) isBonus=true;
    if (!isBonus) // this is unnecessary because fas <= las assertion
      if (minTSpan(SourceFWRuleIdxs[3*idx-2])==MINIMUM_INIT && las<=SourceFWRuleIdxs[3*idx-2]) isBonus=true;
    if (maxdepth1>0) {
      oss << source << "/";
      if (SourceFWRuleAbsIdxs[idx]<=maxdepth1) 
        oss << SourceFWRuleAbsIdxs[idx];
      else 
        oss << "X";
      sourceID = TD::Convert(oss.str());
      oss.str("");
      ScoreOrientationLeft(table,oril,sourceID,SourceFWRuleIdxs[3*idx],
                         isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
    }
    if (maxdepth2>0) {
      oss << source << "/";
      if (maxfwidx-SourceFWRuleAbsIdxs[idx]+1<=maxdepth2)
        oss << ((maxfwidx-SourceFWRuleAbsIdxs[idx]+1)*-1);
      else 
        oss << "X";
      sourceID = TD::Convert(oss.str());
      oss.str("");
      ScoreOrientationLeft(table,oril,sourceID,SourceFWRuleIdxs[3*idx],
                         isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
    } 
    isBonus = false;
    if (las<=SourceFWRuleIdxs[3*idx-2]) isBonus=true;
    if (!isBonus) // this is unnecessary becuase fas <= las assertion
      if (minTSpan(SourceFWRuleIdxs[3*idx-2])==MINIMUM_INIT && SourceFWRuleIdxs[3*idx-2]<=fas) isBonus=true;
    if (maxdepth1>0) {
      oss << source << "/";
      if (SourceFWRuleAbsIdxs[idx]<=maxdepth1)
        oss << SourceFWRuleAbsIdxs[idx];
      else
        oss << "X";
      sourceID = TD::Convert(oss.str());
      oss.str("");
      ScoreOrientationRight(table,orir,sourceID,SourceFWRuleIdxs[3*idx],
                          isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
    }
    if (maxdepth2>0) {
      oss << source << "/";
      if (maxfwidx-SourceFWRuleAbsIdxs[idx]+1<=maxdepth2)
        oss << ((maxfwidx-SourceFWRuleAbsIdxs[idx]+1)*-1);
      else
        oss << "X";
      sourceID = TD::Convert(oss.str());
      oss.str("");
      ScoreOrientationRight(table,orir,sourceID,SourceFWRuleIdxs[3*idx],
                          isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
    }

  }
  for (int i_ant=0; i_ant<_Arity; i_ant++) {
    for (int idx=1; idx<=SourceFWAntsIdxs[i_ant][0]; idx++) {
      if (DEBUG)
        cerr << "considering SourceFWAntsIdxs[" << i_ant << "][" << idx << "]: " << SourceFWAntsIdxs[i_ant][3*idx-2] << endl;
      //if (!((SourceFWAntsAbsIdxs[i_ant][idx]<=maxdepth1)||(maxfwidx-SourceFWAntsAbsIdxs[i_ant][idx]+1<=maxdepth2))) continue;
      // antfas -> first aligned source word antecedent-wise
      // antlas -> last aligned source word antecedent-wise
      int antfas = firstSourceAligned(SourceAntsIdxs[i_ant][1]);
      int antlas = lastSourceAligned(SourceAntsIdxs[i_ant][SourceAntsIdxs[i_ant][0]]);
      if (DEBUG) cerr << " SourceFWAntsAbsIdxs[i_ant][3*idx-1]=" << SourceFWAntsAbsIdxs[i_ant][3*idx-1] << endl;
      string source = TD::Convert(SourceFWAntsIdxs[i_ant][3*idx-1]); 
      assert(antfas <= antlas);
      bool aligned = (minTSpan(SourceFWAntsIdxs[i_ant][3*idx-2])!=MINIMUM_INIT);
      bool Lcompute = true;bool Rcompute = true;
      if ((aligned && antfas<SourceFWAntsIdxs[i_ant][3*idx-2]) ||
          (!aligned && antfas < SourceFWAntsIdxs[i_ant][3*idx-2] && SourceFWAntsIdxs[i_ant][3*idx-2] < antlas))
          Lcompute=false;
      if ((aligned && SourceFWAntsIdxs[i_ant][3*idx-2]<antlas) ||
          (!aligned && antfas < SourceFWAntsIdxs[i_ant][3*idx-2] && SourceFWAntsIdxs[i_ant][3*idx-2] < antlas))
          Rcompute=false;
      if (!Lcompute && !Rcompute) continue;
      OrientationSource(SourceFWAntsIdxs[i_ant][3*idx-2],&oril,&orir,Lcompute, Rcompute);
      bool isBonus = false;
      if (Lcompute) {
        if (SourceFWAntsIdxs[i_ant][3*idx-2]<=fas) isBonus = true;
        //if (!isBonus)  // this is unnecessary
        //  if (!aligned && las<=SourceFWAntsIdxs[i_ant][3*idx-2]) isBonus=true;
        if (maxdepth1>0) {
          oss << source << "/";
          if (SourceFWAntsAbsIdxs[i_ant][idx]<=maxdepth1)
            oss << SourceFWAntsAbsIdxs[i_ant][idx];
          else
            oss << "X";
          sourceID = TD::Convert(oss.str());
          oss.str("");
          ScoreOrientationLeft(table,oril,sourceID,SourceFWAntsIdxs[i_ant][3*idx],
                             isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
        }
        if (maxdepth2>0) {
          oss << source << "/";
          if (maxfwidx-SourceFWAntsAbsIdxs[i_ant][idx]+1<=maxdepth2)
            oss << ((maxfwidx-SourceFWAntsAbsIdxs[i_ant][idx]+1)*-1);
          else
            oss << "X";
          sourceID = TD::Convert(oss.str());
          oss.str("");
          ScoreOrientationLeft(table,oril,sourceID,SourceFWAntsIdxs[i_ant][3*idx],
                             isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
        }
      }
      isBonus = false;
      if (Rcompute) {
        if (las<=SourceFWAntsIdxs[i_ant][3*idx-2]) isBonus = true;
        //if (!isBonus) // this is unnecessary
        //  if (!aligned && SourceFWAntsIdxs[i_ant][3*idx-2]<=fas) isBonus=true;
        if (maxdepth1>0) {
          oss << source << "/";
          if (SourceFWAntsAbsIdxs[i_ant][idx]<=maxdepth1)
            oss << SourceFWAntsAbsIdxs[i_ant][idx];
          else
            oss << "X";
          sourceID = TD::Convert(oss.str());
          oss.str("");
          ScoreOrientationRight(table,orir,sourceID,SourceFWAntsIdxs[i_ant][3*idx],
                              isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
        }	
       if (maxdepth2>0) {
          oss << source << "/";
          if (maxfwidx-SourceFWAntsAbsIdxs[i_ant][idx]+1<=maxdepth2)
            oss << ((maxfwidx-SourceFWAntsAbsIdxs[i_ant][idx]+1)*-1);
          else
            oss << "X";
          sourceID = TD::Convert(oss.str());
          oss.str("");
          ScoreOrientationRight(table,orir,sourceID,SourceFWAntsIdxs[i_ant][3*idx],
                              isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
        }	
      }
    }
  }
}

void Alignment::computeOrientationSource(const CountTable& table, double *cost, double *bonus, 
                                         double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus) {
// a bit complex due to imperfect state (TO DO!!!)
// 1. there are cases where function word alignments come from antecedents, which orientation 
// (either its left or its right) has been computed earlier.   
// 2. some orientation will go as bonus
  if (DEBUG) cerr << "computeOrientationSource" << endl;
  int oril, orir;
  for (int idx=1; idx<=SourceFWRuleIdxs[0]; idx++) {
    if (DEBUG) cerr << "considering SourceFWRuleIdxs[" << idx << "]: " << SourceFWRuleIdxs[3*idx-2] << endl; 
    OrientationSource(SourceFWRuleIdxs[3*idx-2],&oril,&orir);
    bool isBonus = false; // fas -> first aligned source word, las -> last aligned source word
    if (SourceFWRuleIdxs[3*idx-2]<=fas) isBonus=true;
    if (!isBonus) // this is unnecessary because fas <= las assertion 
      if (minTSpan(SourceFWRuleIdxs[3*idx-2])==MINIMUM_INIT && las<=SourceFWRuleIdxs[3*idx-2]) isBonus=true;
    ScoreOrientationLeft(table,oril,SourceFWRuleIdxs[3*idx-1],SourceFWRuleIdxs[3*idx],
                         isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris); 
    isBonus = false;
    if (las<=SourceFWRuleIdxs[3*idx-2]) isBonus=true;
    if (!isBonus) // this is unnecessary becuase fas <= las assertion
      if (minTSpan(SourceFWRuleIdxs[3*idx-2])==MINIMUM_INIT && SourceFWRuleIdxs[3*idx-2]<=fas) isBonus=true;
    ScoreOrientationRight(table,orir,SourceFWRuleIdxs[3*idx-1],SourceFWRuleIdxs[3*idx],
                          isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
  }
  for (int i_ant=0; i_ant<_Arity; i_ant++) {
    for (int idx=1; idx<=SourceFWAntsIdxs[i_ant][0]; idx++) {
      if (DEBUG)
        cerr << "considering SourceFWAntsIdxs[" << i_ant << "][" << idx << "]: " << SourceFWAntsIdxs[i_ant][3*idx-2] << endl;
      // antfas -> first aligned source word antecedent-wise
      // antlas -> last aligned source word antecedent-wise
      int antfas = firstSourceAligned(SourceAntsIdxs[i_ant][1]);
      int antlas = lastSourceAligned(SourceAntsIdxs[i_ant][SourceAntsIdxs[i_ant][0]]);
      assert(antfas <= antlas);
      bool aligned = (minTSpan(SourceFWAntsIdxs[i_ant][3*idx-2])!=MINIMUM_INIT);
      bool Lcompute = true;bool Rcompute = true;
      if ((aligned && antfas<SourceFWAntsIdxs[i_ant][3*idx-2]) ||
          (!aligned && antfas < SourceFWAntsIdxs[i_ant][3*idx-2] && SourceFWAntsIdxs[i_ant][3*idx-2] < antlas)) 
          Lcompute=false; 
      if ((aligned && SourceFWAntsIdxs[i_ant][3*idx-2]<antlas) ||
          (!aligned && antfas < SourceFWAntsIdxs[i_ant][3*idx-2] && SourceFWAntsIdxs[i_ant][3*idx-2] < antlas)) 
          Rcompute=false;
      if (!Lcompute && !Rcompute) continue;
      OrientationSource(SourceFWAntsIdxs[i_ant][3*idx-2],&oril,&orir,Lcompute, Rcompute);
      bool isBonus = false;
      if (Lcompute) {
        if (SourceFWAntsIdxs[i_ant][3*idx-2]<=fas) isBonus = true;
        //if (!isBonus)  // this is unnecessary
        //  if (!aligned && las<=SourceFWAntsIdxs[i_ant][3*idx-2]) isBonus=true;
        ScoreOrientationLeft(table,oril,SourceFWAntsIdxs[i_ant][3*idx-1],SourceFWAntsIdxs[i_ant][3*idx],
                             isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris); 
      }
      isBonus = false;
      if (Rcompute) {
        if (las<=SourceFWAntsIdxs[i_ant][3*idx-2]) isBonus = true;
        //if (!isBonus) // this is unnecessary
        //  if (!aligned && SourceFWAntsIdxs[i_ant][3*idx-2]<=fas) isBonus=true;
        ScoreOrientationRight(table,orir,SourceFWAntsIdxs[i_ant][3*idx-1],SourceFWAntsIdxs[i_ant][3*idx],
                              isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
      }
    }
  }
}

void Alignment::computeOrientationSourceGen(const CountTable& table, double *cost, double *bonus,
                                         double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus, const map<WordID,WordID>& tags) {
  if (DEBUG) cerr << "computeOrientationSourceGen" << endl;
  int oril, orir;
  for (int idx=1; idx<=SourceFWRuleIdxs[0]; idx++) {
    if (DEBUG) cerr << "considering SourceFWRuleIdxs[" << idx << "]: " << SourceFWRuleIdxs[3*idx-2] << endl;
    OrientationSource(SourceFWRuleIdxs[3*idx-2],&oril,&orir);
    bool isBonus = false; // fas -> first aligned source word, las -> last aligned source word
    if (SourceFWRuleIdxs[3*idx-2]<=fas) isBonus=true;
    if (!isBonus) // this is unnecessary because fas <= las assertion 
      if (minTSpan(SourceFWRuleIdxs[3*idx-2])==MINIMUM_INIT && las<=SourceFWRuleIdxs[3*idx-2]) isBonus=true;
    ScoreOrientationLeft(table,oril,generalize(SourceFWRuleIdxs[3*idx-1],tags),SourceFWRuleIdxs[3*idx],
                         isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
    isBonus = false;
    if (las<=SourceFWRuleIdxs[3*idx-2]) isBonus=true;
    if (!isBonus) // this is unnecessary becuase fas <= las assertion
      if (minTSpan(SourceFWRuleIdxs[3*idx-2])==MINIMUM_INIT && SourceFWRuleIdxs[3*idx-2]<=fas) isBonus=true;
    ScoreOrientationRight(table,orir,generalize(SourceFWRuleIdxs[3*idx-1],tags),SourceFWRuleIdxs[3*idx],
                          isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
  }
  for (int i_ant=0; i_ant<_Arity; i_ant++) {
    for (int idx=1; idx<=SourceFWAntsIdxs[i_ant][0]; idx++) {
      if (DEBUG)
        cerr << "considering SourceFWAntsIdxs[" << i_ant << "][" << idx << "]: " << SourceFWAntsIdxs[i_ant][3*idx-2] << endl;
      // antfas -> first aligned source word antecedent-wise
      // antlas -> last aligned source word antecedent-wise
      int antfas = firstSourceAligned(SourceAntsIdxs[i_ant][1]);
      int antlas = lastSourceAligned(SourceAntsIdxs[i_ant][SourceAntsIdxs[i_ant][0]]);
      assert(antfas <= antlas);
      bool aligned = (minTSpan(SourceFWAntsIdxs[i_ant][3*idx-2])!=MINIMUM_INIT);
      bool Lcompute = true;bool Rcompute = true;
      if ((aligned && antfas<SourceFWAntsIdxs[i_ant][3*idx-2]) ||
          (!aligned && antfas < SourceFWAntsIdxs[i_ant][3*idx-2] && SourceFWAntsIdxs[i_ant][3*idx-2] < antlas))
          Lcompute=false;
      if ((aligned && SourceFWAntsIdxs[i_ant][3*idx-2]<antlas) ||
          (!aligned && antfas < SourceFWAntsIdxs[i_ant][3*idx-2] && SourceFWAntsIdxs[i_ant][3*idx-2] < antlas))
          Rcompute=false;
      if (!Lcompute && !Rcompute) continue;
      OrientationSource(SourceFWAntsIdxs[i_ant][3*idx-2],&oril,&orir,Lcompute, Rcompute);
      bool isBonus = false;
      if (Lcompute) {
        if (SourceFWAntsIdxs[i_ant][3*idx-2]<=fas) isBonus = true;
        //if (!isBonus)  // this is unnecessary
       //  if (!aligned && las<=SourceFWAntsIdxs[i_ant][3*idx-2]) isBonus=true;
        ScoreOrientationLeft(table,oril,generalize(SourceFWAntsIdxs[i_ant][3*idx-1],tags),SourceFWAntsIdxs[i_ant][3*idx],
                             isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
      }
      isBonus = false;
      if (Rcompute) {
        if (las<=SourceFWAntsIdxs[i_ant][3*idx-2]) isBonus = true;
        //if (!isBonus) // this is unnecessary
        //  if (!aligned && SourceFWAntsIdxs[i_ant][3*idx-2]<=fas) isBonus=true;
        ScoreOrientationRight(table,orir,generalize(SourceFWAntsIdxs[i_ant][3*idx-1],tags),SourceFWAntsIdxs[i_ant][3*idx],
                              isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_oris,beta_oris);
      }
    }
  }
}
void Alignment::computeOrientationTarget(const CountTable& table, double *cost, double *bonus, double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus) {
  if (DEBUG) cerr << "computeOrientationTarget" << endl;
  int oril, orir;
  for (int idx=1; idx<=TargetFWRuleIdxs[0]; idx++) {
    if (DEBUG) cerr << "considering TargetFWRuleIdxs[" << idx << "]: " << TargetFWRuleIdxs[3*idx-2] << endl;
    OrientationTarget(TargetFWRuleIdxs[3*idx-2],&oril,&orir);
    // the second and the third parameters of ScoreOrientationLeft must be e and f (not f and then e) 
    bool isBonus = false;
    if (TargetFWRuleIdxs[3*idx-2]<=fat) isBonus = true;
    if (!isBonus)
      if (minSSpan(TargetFWRuleIdxs[3*idx-2])==MINIMUM_INIT && lat<=TargetFWRuleIdxs[3*idx-2]) isBonus = true;
    ScoreOrientationLeft(table,oril,TargetFWRuleIdxs[3*idx-1],TargetFWRuleIdxs[3*idx],
                         isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_orit,beta_orit); 
    isBonus = false;
    if (lat<=TargetFWRuleIdxs[3*idx-2]) isBonus = true;
    if (!isBonus)
      if (minSSpan(TargetFWRuleIdxs[3*idx-2])==MINIMUM_INIT && TargetFWRuleIdxs[3*idx-2]<=fat) isBonus=true;
    ScoreOrientationRight(table,orir,TargetFWRuleIdxs[3*idx-1],TargetFWRuleIdxs[3*idx],
                          isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_orit,beta_orit);
  }

  for (int i_ant=0; i_ant<_Arity; i_ant++) {
    for (int idx=1; idx<=TargetFWAntsIdxs[i_ant][0]; idx++) {
      if (DEBUG) cerr << "considering TargetFWAntsIdxs[" << i_ant << "][" << idx << "]: " << TargetFWAntsIdxs[i_ant][3*idx-2] << endl; 
      int antfat = firstTargetAligned(TargetAntsIdxs[i_ant][1]);
      int antlat = lastTargetAligned(TargetAntsIdxs[i_ant][TargetAntsIdxs[i_ant][0]]);
      int aligned = (minSSpan( TargetFWAntsIdxs[i_ant][3*idx-2])!=MINIMUM_INIT);
      bool Lcompute = true; bool Rcompute = true;
      if ((aligned && antfat<TargetFWAntsIdxs[i_ant][3*idx-2]) ||
          (!aligned && antfat < TargetFWAntsIdxs[i_ant][3*idx-2] && TargetFWAntsIdxs[i_ant][3*idx-2] < antlat)) 
          Lcompute=false;
      if ((aligned && TargetFWAntsIdxs[i_ant][3*idx-2]<antlat) ||
          (!aligned && antfat < TargetFWAntsIdxs[i_ant][3*idx-2] && TargetFWAntsIdxs[i_ant][3*idx-2] < antlat)) 
          Rcompute=false;
      if (!Lcompute && !Rcompute) continue;
      bool isBonus = false;
      OrientationTarget(TargetFWAntsIdxs[i_ant][3*idx-2],&oril,&orir, Lcompute, Rcompute);
      if (Lcompute) {
        if (TargetFWAntsIdxs[i_ant][3*idx-2]<=fat) isBonus=true;
        //if (!isBonus) 
        //  if (!aligned && lat<=TargetFWAntsIdxs[i_ant][3*idx-2]) isBonus=true;
        ScoreOrientationLeft(table,oril,TargetFWAntsIdxs[i_ant][3*idx-1],TargetFWAntsIdxs[i_ant][3*idx],
                             isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_orit,beta_orit); 
      }
      isBonus = false;
      if (Rcompute) {
        if (lat<=TargetFWAntsIdxs[i_ant][3*idx-2]) isBonus=true;
        if (!isBonus)
          //if (!aligned && TargetFWAntsIdxs[i_ant][3*idx-2]<=fat) isBonus=true;
        ScoreOrientationRight(table,orir,TargetFWAntsIdxs[i_ant][3*idx-1],TargetFWAntsIdxs[i_ant][3*idx],
                              isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_orit,beta_orit);
      }
    }
  }
}

void Alignment::computeOrientationTargetBackward(const CountTable& table, double *cost, double *bonus, double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus) {
  if (DEBUG) cerr << "computeOrientationTargetBackward" << endl;
  int oril, orir;
  for (int idx=1; idx<=TargetFWRuleIdxs[0]; idx++) {
    if (DEBUG) cerr << "considering TargetFWRuleIdxs[" << idx << "]: " << TargetFWRuleIdxs[3*idx-2] << endl;
    int* fwblock = blockSource(TargetFWRuleIdxs[3*idx-2],TargetFWRuleIdxs[3*idx-2]);
    bool aligned = (fwblock[0] == MINIMUM_INIT);
    if (aligned) {
      OrientationSource(fwblock[0],fwblock[1],&oril,&orir);
    } else {
      OrientationTarget(TargetFWRuleIdxs[3*idx-2],&oril,&orir);
    }
    delete fwblock;
    // the second and the third parameters of ScoreOrientationLeft must be e and f (not f and then e)
    bool isBonus = false;
    if (TargetFWRuleIdxs[3*idx-2]<=fat) isBonus = true;
    //if (!isBonus)  // unnecessary
      //if (minSSpan(TargetFWRuleIdxs[3*idx-2])==MINIMUM_INIT && lat<=TargetFWRuleIdxs[3*idx-2]) isBonus = true;
    ScoreOrientationLeftBackward(table,oril,TargetFWRuleIdxs[3*idx-1],TargetFWRuleIdxs[3*idx],
                         isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_orit,beta_orit);
    isBonus = false;
    if (lat<=TargetFWRuleIdxs[3*idx-2]) isBonus = true;
    //if (!isBonus) // unnecessary
      //if (minSSpan(TargetFWRuleIdxs[3*idx-2])==MINIMUM_INIT && TargetFWRuleIdxs[3*idx-2]<=fat) isBonus=true;
    ScoreOrientationRightBackward(table,orir,TargetFWRuleIdxs[3*idx-1],TargetFWRuleIdxs[3*idx],
                          isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_orit,beta_orit);
  }

  for (int i_ant=0; i_ant<_Arity; i_ant++) {
    int antfat = firstTargetAligned(TargetAntsIdxs[i_ant][1]);
    int antlat = lastTargetAligned(TargetAntsIdxs[i_ant][TargetAntsIdxs[i_ant][0]]);
    int antfas = firstSourceAligned(SourceAntsIdxs[i_ant][1]);
    int antlas = lastSourceAligned(SourceAntsIdxs[i_ant][SourceAntsIdxs[i_ant][0]]);
    for (int idx=1; idx<=TargetFWAntsIdxs[i_ant][0]; idx++) {
      if (DEBUG) cerr << "considering TargetFWAntsIdxs[" << i_ant << "][" << idx << "]: " << TargetFWAntsIdxs[i_ant][3*idx-2] << endl;
      int* fwblock = blockTarget(TargetFWAntsIdxs[i_ant][3*idx-2],TargetFWAntsIdxs[i_ant][3*idx-2]);
      bool aligned = (fwblock[0]!=MINIMUM_INIT);
      //bool aligned = (minSSpan( TargetFWAntsIdxs[i_ant][3*idx-2])!=MINIMUM_INIT);
      bool Lcompute = true; bool Rcompute = true;
      if ((aligned && antfas<fwblock[0]) ||
          (!aligned && antfat < fwblock[2]))
          Lcompute=false;
      if ((aligned && fwblock[0]<antlas) ||
          (!aligned && fwblock[3] < antlat))
          Rcompute=false;
      if (!Lcompute && !Rcompute) continue;
      bool isBonus = false;
      if (aligned) {
        OrientationSource(fwblock[0],fwblock[1],&oril,&orir,Lcompute,Rcompute);
      } else { 
        OrientationTarget(TargetFWAntsIdxs[i_ant][3*idx-2],&oril,&orir, Lcompute, Rcompute);
      }
      if (Lcompute) {
        if ((aligned && fwblock[1]<=fas) ||
            (!aligned && fwblock[3]<=fat))
            isBonus=true;
        //if (!isBonus)
        //  if (!aligned && lat<=TargetFWAntsIdxs[i_ant][3*idx-2]) isBonus=true;
        ScoreOrientationLeftBackward(table,oril,TargetFWAntsIdxs[i_ant][3*idx-1],TargetFWAntsIdxs[i_ant][3*idx],
                             isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_orit,beta_orit);
      }
      isBonus = false;
      if (Rcompute) {
        if ((aligned && las<=fwblock[0]) ||
            (!aligned && lat<=fwblock[2]))
            isBonus=true;
        if (!isBonus)
          //if (!aligned && TargetFWAntsIdxs[i_ant][3*idx-2]<=fat) isBonus=true;
        ScoreOrientationRightBackward(table,orir,TargetFWAntsIdxs[i_ant][3*idx-1],TargetFWAntsIdxs[i_ant][3*idx],
                              isBonus,cost,bonus,bo1,bo1_bonus,bo2,bo2_bonus,alpha_orit,beta_orit);
      }
      delete fwblock;
    }
  }
}

bool Alignment::MemberOf(int* FWIdxs, int pos1, int pos2) {
  for (int idx=2; idx<=FWIdxs[0]; idx++) { 
    if (FWIdxs[3*(idx-1)-2]==pos1 && FWIdxs[3*idx-2]==pos2) return true;
  }
  return false;
}

void Alignment::computeDominanceSource(const CountTable& table, WordID lfw, WordID rfw, 
     double *cost, double *bonus, double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus) {
  // no bonus yet
  if (DEBUG) cerr << "computeDominanceSource" << endl;
  if (DEBUG) cerr << " initial cost=" << *cost << ", initial bonus=" << *bonus << endl;
  for (int idx=2; idx<=SourceFWIdxs[0]; idx++) {
    if (DEBUG) { 
      cerr << "PrevSourceFWIdxs :" << SourceFWIdxs[3*(idx-1)-2] << "," << SourceFWIdxs[3*(idx-1)-1] 
           << "," << SourceFWIdxs[3*(idx-1)] << endl;
      cerr << "CurrSourceFWIdxs :" << SourceFWIdxs[3*(idx)-2] << "," << SourceFWIdxs[3*(idx)-1] 
           << "," << SourceFWIdxs[3*(idx)] << endl;
    }
    bool compute = true;
    for (int i_ant=0; i_ant<_Arity && compute; i_ant++) {
      if (MemberOf(SourceFWAntsIdxs[i_ant],SourceFWIdxs[3*(idx-1)-2],SourceFWIdxs[3*(idx)-2])) {
        //cerr << "Skipping, they have been calculated in the  " << (i_ant+1) << "-th branch" << endl;
        compute=false;
      }
    }
    if (compute) {
      int dom = DominanceSource(SourceFWIdxs[3*(idx-1)-2],SourceFWIdxs[3*idx-2]);
      if (DEBUG) cerr << "dom = " << dom << endl;
      ScoreDominance(table,dom,SourceFWIdxs[3*(idx-1)-1],SourceFWIdxs[3*idx-1],SourceFWIdxs[3*(idx-1)],SourceFWIdxs[3*idx],
              cost,bo1,bo2,false,alpha_doms,beta_doms);
      if (DEBUG) cerr << "cost now is " << *cost << endl;
    }
  }
  if (SourceFWIdxs[0]>0) {
    if (lfw>=0) {
      int dom = DominanceSource(0,SourceFWIdxs[1]);
      if (DEBUG) cerr << " --> lfw = " << lfw << "-" << TD::Convert(lfw) << endl;
      if (DEBUG) cerr << " --> rfw = " << rfw << "-" << TD::Convert(rfw) << endl;
      ScoreDominance(table,dom,lfw,SourceFWIdxs[2],lfw,SourceFWIdxs[3],bonus,bo1_bonus,bo2_bonus,true,alpha_doms,beta_doms);
    }
    if (rfw>=0) {
      int dom = DominanceSource(SourceFWIdxs[3*SourceFWIdxs[0]-2],_J-1);
      ScoreDominance(table,dom,SourceFWIdxs[3*SourceFWIdxs[0]-1],rfw,SourceFWIdxs[3*SourceFWIdxs[0]],
                     rfw,bonus,bo1_bonus,bo2_bonus,true,alpha_doms,beta_doms);
    }
  }
}

void Alignment::computeDominanceSourcePos(const CountTable& table, WordID lfw, WordID rfw,
     double *cost, double *bonus, double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus, int maxfwidx, int maxdepth1, int maxdepth2) {
  if (DEBUG) cerr << "computeDominanceSourcePos" << endl;
  if (DEBUG) cerr << " initial cost=" << *cost << ", initial bonus=" << *bonus << endl;
  ostringstream oss;
  for (int idx=2; idx<=SourceFWIdxs[0]; idx++) {
    if (DEBUG) {
      cerr << "PrevSourceFWIdxs :" << SourceFWIdxs[3*(idx-1)-2] << "," << SourceFWIdxs[3*(idx-1)-1]
           << "," << SourceFWIdxs[3*(idx-1)] << endl;
      cerr << "CurrSourceFWIdxs :" << SourceFWIdxs[3*(idx)-2] << "," << SourceFWIdxs[3*(idx)-1]
           << "," << SourceFWIdxs[3*(idx)] << endl;
    }
    //if (!((SourceFWAbsIdxs[3*(idx-1)-2]<=maxdepth1 && SourceFWAbsIdxs[3*idx-2]<=maxdepth1) ||
    //      (maxfwidx-SourceFWAbsIdxs[3*(idx-1)-2]+1<=maxdepth2 && maxfwidx-SourceFWAbsIdxs[3*idx-2]+1<=maxdepth2))) continue;
    bool compute = true;
    for (int i_ant=0; i_ant<_Arity && compute; i_ant++) {
      if (MemberOf(SourceFWAntsIdxs[i_ant],SourceFWIdxs[3*(idx-1)-2],SourceFWIdxs[3*(idx)-2])) {
        //cerr << "Skipping, they have been calculated in the  " << (i_ant+1) << "-th branch" << endl;
        compute=false;
      }
    }
    if (compute) {
      int dom = DominanceSource(SourceFWIdxs[3*(idx-1)-2],SourceFWIdxs[3*idx-2]);
      if (DEBUG) cerr << "dom = " << dom << endl;
      if (maxdepth1+maxdepth2>0) {
        string source1 = TD::Convert(SourceFWIdxs[3*(idx-1)-1]);
        string source2 = TD::Convert(SourceFWIdxs[3*(idx)-1]);
        if (maxdepth1>0) {
          oss << source1 << "/";
          if (SourceFWAbsIdxs[3*(idx-1)-2]<=maxdepth1) 
            oss << SourceFWAbsIdxs[3*(idx-1)-2];
          else 
            oss << "X";
          WordID source1id = TD::Convert(oss.str());
          oss.str("");
          oss << source2 << "/";
          if (SourceFWAbsIdxs[3*idx-2]<=maxdepth1)
            oss << SourceFWAbsIdxs[3*idx-2];
          else
            oss << "X";
          WordID source2id = TD::Convert(oss.str());
          oss.str("");    
          ScoreDominance(table,dom,source1id,source2id,SourceFWIdxs[3*(idx-1)],SourceFWIdxs[3*idx],
              cost,bo1,bo2,false,alpha_doms,beta_doms);
        }  
        if (maxdepth2>0) {
          oss << source1 << "/";
          if (maxfwidx-SourceFWAbsIdxs[3*(idx-1)-2]+1<=maxdepth2) 
            oss << ((maxfwidx-SourceFWAbsIdxs[3*(idx-1)-2]+1)*-1);
          else
            oss << "X";
          WordID source1id = TD::Convert(oss.str());
          oss.str("");
          oss << source2 << "/";
          if (maxfwidx-SourceFWAbsIdxs[3*idx-2]+1<=maxdepth2) 
            oss << ((maxfwidx-SourceFWAbsIdxs[3*(idx-1)-2]+1)*-1);
          else
            oss << "X";
          WordID source2id = TD::Convert(oss.str());
          oss.str("");
          ScoreDominance(table,dom,source1id,source2id,SourceFWIdxs[3*(idx-1)],SourceFWIdxs[3*idx],
              cost,bo1,bo2,false,alpha_doms,beta_doms);
        }
      }
    }
  }
  if (SourceFWIdxs[0]>0) {
    if (lfw>=0) {
      int dom = DominanceSource(0,SourceFWIdxs[1]);
      string source1 = TD::Convert(lfw);
      string source2 = TD::Convert(SourceFWIdxs[2]);
      if (maxdepth1>0) {
        oss << source1 << "/";
        if (SourceFWAbsIdxs[1]-1<=maxdepth1) 
          oss << (SourceFWAbsIdxs[1]-1);
        else
          oss << "X";
        WordID source1id = TD::Convert(oss.str());
        oss.str("");
        oss << source2 << "/";
        if (SourceFWAbsIdxs[1]<=maxdepth1)
          oss << SourceFWAbsIdxs[1];
        else
          oss << "X";
        WordID source2id = TD::Convert(oss.str());
        oss.str("");
        ScoreDominance(table,dom,source1id,source2id,lfw,SourceFWIdxs[3],bonus,bo1_bonus,bo2_bonus,true,alpha_doms,beta_doms);
      }
      if (maxdepth2>0) { 
        oss << source1 << "/";
        if (maxfwidx-(SourceFWAbsIdxs[1]-1)+1<=maxdepth2) 
          oss << ((maxfwidx-(SourceFWAbsIdxs[1]-1)+1)*-1);
        else 
          oss << "X";
        WordID source1id = TD::Convert(oss.str());
        oss.str("");
        oss << source2 << "/";
        if (maxfwidx-SourceFWAbsIdxs[1]+1<=maxdepth2) 
          oss << ((maxfwidx-SourceFWAbsIdxs[1]+1)*-1);
        else 
          oss << "X";
        WordID source2id = TD::Convert(oss.str());
        oss.str("");
        ScoreDominance(table,dom,source1id,source2id,lfw,SourceFWIdxs[3],bonus,bo1_bonus,bo2_bonus,true,alpha_doms,beta_doms);
      }
    }
    if (rfw>=0) {
      int dom = DominanceSource(SourceFWIdxs[3*SourceFWIdxs[0]-2],_J-1);
      string source1 = TD::Convert(SourceFWIdxs[3*SourceFWIdxs[0]-1]);
      string source2 = TD::Convert(rfw);
      if (maxdepth1>0) {
        oss << source1 << "/";
        if (SourceFWAbsIdxs[3*SourceFWAbsIdxs[0]-2]<=maxdepth1)
          oss << SourceFWAbsIdxs[3*SourceFWIdxs[0]-2];
        else
          oss << "X";
        WordID source1id = TD::Convert(oss.str());
        oss.str("");
        oss << source2 << "/";
        if (SourceFWAbsIdxs[3*SourceFWAbsIdxs[0]-2]+1<=maxdepth1)
          oss << (SourceFWAbsIdxs[3*SourceFWAbsIdxs[0]-2]+1);
        else 
          oss << "X";
        WordID source2id = TD::Convert(oss.str());
        ScoreDominance(table,dom,source1id,source2id,SourceFWIdxs[3*SourceFWIdxs[0]],
                     rfw,bonus,bo1_bonus,bo2_bonus,true,alpha_doms,beta_doms);
      }
      if (maxdepth2>0) {
        oss << source1 << "/";
        if (maxfwidx-SourceFWAbsIdxs[3*SourceFWAbsIdxs[0]-2]+1<=maxdepth2) 
          oss << ((maxfwidx-SourceFWAbsIdxs[3*SourceFWAbsIdxs[0]-2]+1)*-1);
        else 
          oss << "X";
        WordID source1id = TD::Convert(oss.str());
        oss.str("");
        oss << source2 << "/";
        if (maxfwidx-(SourceFWAbsIdxs[3*SourceFWAbsIdxs[0]-2]+1)+1<=maxdepth2) 
          oss << ((maxfwidx-(SourceFWAbsIdxs[3*SourceFWAbsIdxs[0]-2]+1)+1)*-1);
        else 
          oss << "X";
        WordID source2id = TD::Convert(oss.str());
        oss.str("");
        ScoreDominance(table,dom,source1id,source2id,SourceFWIdxs[3*SourceFWIdxs[0]],
                     rfw,bonus,bo1_bonus,bo2_bonus,true,alpha_doms,beta_doms);
      }
    }
  }
}


void Alignment::computeDominanceTarget(const CountTable& table, WordID lfw, WordID rfw,
     double *cost, double *bonus, double *bo1, double *bo1_bonus, double *bo2, double *bo2_bonus) {
   if (DEBUG) cerr << "computeDominanceTarget" << endl;
  for (int idx=2; idx<=TargetFWIdxs[0]; idx++) {
    if (DEBUG) cerr << "PrevTargetFWIdxs :" << TargetFWIdxs[3*(idx-1)-2] << "," << TargetFWIdxs[3*(idx-1)-1] << "," <<TargetFWIdxs[3*(idx-1)] << endl;
    if (DEBUG) cerr << "CurrTargetFWIdxs :" << TargetFWIdxs[3*(idx)-2] << "," << TargetFWIdxs[3*(idx)-1] << "," <<TargetFWIdxs[3*(idx)] << endl;
    bool compute = true;
    for (int i_ant=0; i_ant <_Arity && compute; i_ant++) {
      if (MemberOf(TargetFWAntsIdxs[i_ant],TargetFWIdxs[3*(idx-1)-2],TargetFWIdxs[3*idx-2])) {
        if (DEBUG) cerr << "Skipping, they have been calculated in the " << (i_ant+1) << "-th branch" << endl;
        compute = false;
      }
    }
    if (compute) {
      int dom = DominanceTarget(TargetFWIdxs[3*(idx-1)-2],TargetFWIdxs[3*idx-2]);
      //cerr << (3*(idx-1)) << "," << (3*idx) << "," << (3*(idx-1)-1) << "," << (3*idx-1) << endl;
      if (DEBUG) cerr << "dom target = " << dom << endl;
      ScoreDominance(table,dom,TargetFWIdxs[3*(idx-1)],TargetFWIdxs[3*idx],TargetFWIdxs[3*(idx-1)-1],TargetFWIdxs[3*idx-1],
              cost,bo1,bo2,false,alpha_domt,beta_domt);
    }
  }
  if (TargetFWIdxs[0]>0) {
    if (DEBUG) cerr << "backoff dominance " << endl;
    if (lfw>=0) {
      int dom = DominanceTarget(0,TargetFWIdxs[1]);
      if (DEBUG) cerr << "dom target (with left) = " << dom << endl;
      ScoreDominance(table,dom,lfw,lfw,TargetFWIdxs[2],TargetFWIdxs[3],bonus,bo1_bonus,bo2_bonus,true,alpha_domt,beta_domt);
    }
    if (rfw>=0) {
      int dom = DominanceTarget(TargetFWIdxs[3*TargetFWIdxs[0]-2],_I-1);
      if (DEBUG) cerr << "dom target (with right) = " << dom << endl;
      ScoreDominance(table,dom,TargetFWIdxs[3*TargetFWIdxs[0]-1],TargetFWIdxs[3*TargetFWIdxs[0]],
                     rfw,rfw,bonus,bo1_bonus,bo2_bonus,true,alpha_domt,beta_domt);
    }
  }

  //cerr << "END of computeDominanceTarget" << endl;
}

double Alignment::ScoreDominance(const CountTable& table, int dom, WordID source1, WordID source2, WordID target1, WordID target2) {
  if (DEBUG) {
     cerr << "ScoreDominance(source1=" << TD::Convert(source1) << ",source2=" << TD::Convert(source2)
          << ",target1=" << TD::Convert(target1) << ",target2=" << TD::Convert(target2) << ", dom=" << dom << endl;
  }
  string _source1 = TD::Convert(source1);
  string _source2 = TD::Convert(source2);
  string _source1idx; string _source2idx;
  if (table.mode==1) {
    _source1idx = _source1; _source2idx = _source2;
    _source1 = _source1idx.substr(0,_source1idx.find_last_of("/"));
    _source2 = _source2idx.substr(0,_source2idx.find_last_of("/"));
  }
  string _target1 = TD::Convert(target1);
  string _target2 = TD::Convert(target2);

  double count = table.ultimate[dom];
  double total = table.ultimate[4];
  double prob = count/total;
  if (DEBUG) cerr << "level0 " << count << "/" << total << "=" << prob << endl;
  double alpha = 0.1;
  
  string key = _source1 + " " + _source2;
  WordID key_id = TD::Convert(key);
  map<WordID,int*>::const_iterator it = table.model.find(key_id); 
  bool stop = (it==table.model.end());
  if (!stop) {
    stop = true;
    if (it->second[4]>=0) {
      count = it->second[dom] + alpha*prob;
      total = it->second[4]   + alpha;
      prob = count/total;
      if (DEBUG) cerr << "level1 " << count << "/" << total << "=" << prob << endl;
      stop = false;
    }
  } 
  if (stop) return prob;
 
  key = _source1 + " " + _source2 + " " + _target1 + " " + _target2; 
  key_id = TD::Convert(key);  
  it = table.model.find(key_id);
  stop = (it==table.model.end());
  if (!stop) {
    stop = true;
    if (it->second[4]>=0) {
      count = it->second[dom] + alpha*prob;
      total = it->second[4]   + alpha;
      prob = count/total;
      if (DEBUG) cerr << "level2 " << count << "/" << total << "=" << prob << endl;
      stop = false;
    }
  } 
 
  if (table.mode!=1 || stop) return prob;
  key = _source1 + " " + _source2 + " " + _target1 + " " + _target2;
  key_id = TD::Convert(key);
  it = table.model.find(key_id);
  if (it!=table.model.end()) {
    if (it->second[4]>=0) {
      count = it->second[dom] + alpha*prob;
      total = it->second[4]   + alpha;
      if (DEBUG) cerr << "level3 " << count << "/" << total << "=" << prob << endl;
      prob = count/total;
    }
  }

  return prob;
}

void Alignment::ScoreDominance(const CountTable& table, int dom, WordID source1, WordID source2, WordID target1, WordID target2, double *cost, double *bo1, double *bo2, bool isBonus, double alpha2, double beta2) {
  if (DEBUG) 
    cerr << "ScoreDominance(source1=" << TD::Convert(source1) << ",source2=" << TD::Convert(source2) 
         << ",target1=" << TD::Convert(target1) << ",target2=" << TD::Convert(target2) << ",isBonus=" << isBonus << ", alpha2 = " << alpha2 << ", beta2 = " << beta2 << endl;
  if (DEBUG) cerr << "    BEFORE=" << *cost << endl;
  *cost += ScoreDominance(table,dom,source1,source2,target1,target2);
  if (DEBUG) cerr << "    AFTER=" << *cost << endl;
}

WordID Alignment::F2EProjectionFromExternal(int idx, const vector<AlignmentPoint>& als, const string& delimiter) {
  if (DEBUG) {
    cerr << "F2EProjectionFromExternal=" << idx << endl;
    for (int i=0; i< als.size(); i++) cerr << "als[" << i << "]=" << als[i] << " ";
    cerr << endl;
  }
  vector<int> alignedTo;
  for (int i=0; i<als.size(); i++) {
    if (DEBUG) cerr << als[i] << " ";
    if (als[i].s_==idx) 
      alignedTo.push_back(als[i].t_);
  }
  if (DEBUG) {
    cerr << endl;
    cerr << "alignedTo = ";
    for (int i=0; i<alignedTo.size(); i++) cerr << alignedTo[i] << " ";
    cerr << endl;
  }
  if (alignedTo.size()==0) {
    if (DEBUG) cerr << "returns [NULL] : " << TD::Convert("NULL") << endl;
    return TD::Convert("NULL");
  } else if (alignedTo.size()==1) {
    if (DEBUG) cerr << "returns [" << TD::Convert(_e[alignedTo[0]]) << "] : " << _e[alignedTo[0]] << endl;
    return _e[alignedTo[0]]; // if not aligned to many, why bother continuing
  } else {
    ostringstream projection;
    for (int i=0; i<alignedTo.size(); i++) {
      if (i>0) projection << delimiter;
      projection << TD::Convert(_e[alignedTo[i]]);
    }
    if (DEBUG) {
      cerr << "projection = " << projection.str() << endl;
      cerr << "returns = " << TD::Convert(projection.str()) << endl;
    }
    return TD::Convert(projection.str());
  }
}

WordID Alignment::E2FProjectionFromExternal(int idx, const vector<AlignmentPoint>& als, const string& delimiter) {
  vector<int> alignedTo;
  for (int i=0; i<als.size(); i++)
    if (als[i].t_==idx) alignedTo.push_back(als[i].s_);
  if (alignedTo.size()==0) {
    return TD::Convert("NULL");
  } else if (alignedTo.size()==1) {
    return _f[alignedTo[0]]; // if not aligned to many, why bother continuing
  } else {
    ostringstream projection;
    for (int i=0; i<alignedTo.size(); i++) {
      if (i>0) projection << delimiter;
      projection << TD::Convert(_f[alignedTo[i]]);
    }
    return TD::Convert(projection.str());
  }
}


WordID Alignment::F2EProjection(int idx, const string& delimiter) {
  if (DEBUG) cerr << "F2EProjection(" << idx << ")" << endl;
  int e = targetOf(idx);
  if (e<0) {
    if (DEBUG) cerr << "projection = NULL" << endl;
    return TD::Convert("NULL");
  } else {
    if (targetOf(idx,e+1)<0) {
      if (DEBUG) cerr << "e-1=" << (e-1) << ", size=" << _e.size() << endl;
      return getE(e-1); // if not aligned to many, why bother continuing
    }
    ostringstream projection;
    bool firstTime = true;
    do {
      if (!firstTime) projection << delimiter; 
      projection << TD::Convert(_e[e-1]); // transform space
      firstTime = false;
      e = targetOf(idx,e+1);
      //if (DEBUG) cerr << "projection = " << projection.str() << endl;
    } while(e>=0);
    return TD::Convert(projection.str());
  }
}

WordID Alignment::E2FProjection(int idx, const string& delimiter) {
  //cerr << "E2FProjection(" << idx << ")" << endl;
  //cerr << "i" << endl;
  int f = sourceOf(idx);
  //cerr << "j, f=" << f << endl;
  if (f<0) {
    //cerr << "projection = NULL" << endl;
    return TD::Convert("NULL");
  } else {
    if (sourceOf(idx,f+1)<0) return getF(f-1);
    bool firstTime = true;
    ostringstream projection(ostringstream::out);
    do {
      if (!firstTime) projection << delimiter;
      projection << TD::Convert(_f[f-1]); //transform space
      firstTime = false;
      f = sourceOf(idx,f+1);
      //cerr << "projection = " << projection.str() << endl;
    } while(f>=0);
    return TD::Convert(projection.str());
  }
}
void Alignment::computeBorderDominanceSource(const CountTable& table, double *cost, double *bonus, double *state_mono, 
        double *state_nonmono, TRule &rule, const std::vector<const void*>& ant_contexts, const map<WordID,int>& sfw) {
  // HACK: GOAL is assumed to always be "S"
  if (DEBUG) cerr << "computeBorderDominanceSource" << endl;
  std::vector<WordID> f = rule.f();
  std::vector<WordID> e = rule.e();
  int nt_index[f.size()];
  int nt_count=0;
  for (int i=0; i<f.size(); i++) nt_index[i] = (f[i]<0)? ++nt_count : 0;
  if (DEBUG) {
    cerr << "f = ";
    for (int i=0; i<f.size(); i++) cerr << i << "." << "[" << f[i] << "] ";
    cerr << endl;
    cerr << "e = ";
    for (int i=0; i<e.size(); i++) cerr << i << "." << "[" << e[i] << "] ";
    cerr << endl;
  }
  bool flag[f.size()]; 
  for (int idx=0; idx<f.size(); idx++) flag[idx]=false;
  //collect alignments
  vector<int> als;
  for (std::vector<AlignmentPoint>::const_iterator i = rule.als().begin(); i != rule.als().end(); ++i) {
    int s = i->s_; int t = i->t_;
    als.push_back(link(t,s));
  }
  if (DEBUG) cerr << "rule.Arity=" << rule.Arity() << endl;
  if (rule.Arity()>0) {
    int ntc=0;
    for (int s=0; s<f.size(); s++) {
      if (f[s]<=0) {
        if (DEBUG) cerr << "f[s]=" << f[s] << "+" << s << " - ";
        for (int t=0; t<e.size(); t++) {
          if (e[t]==ntc) {
            if (DEBUG) cerr << "e[t]=" << e[t] << "+" << t <<endl;
            als.push_back(link(t,s));
            ntc--;  break;
          }
        }
      } 
    }
  }
  if (DEBUG) {
    cerr << "unsorted alignments (nonterminals and terminals)" << endl;
    for (int i=0; i<als.size(); i++)
      cerr << source(als[i]) << "-" << target(als[i]) << " ";
    cerr << endl;
  }
  // sort alignments according to target
  std::sort(als.begin(),als.end());
  if (DEBUG) {
    cerr << "sorted alignments (nonterminals and terminals)" << endl;
    for (int i=0; i<als.size(); i++) 
      cerr << source(als[i]) << "-" << target(als[i]) << " ";
    cerr << endl;
  }
  // 0 -> neither, 1 -> leftFirst, 2 -> rightFirst, 3 -> dontCare
  // ScoreDominance(const CountTable& table, int dom, WordID source1, WordID source2, WordID target1, WordID target2)
  int prevs = 0;
  for (int i=0; i<als.size(); i++) {
    int currs = target(als[i]); //int currt = source(als[i]);
    if (DEBUG) cerr << "prevs=" << prevs << ", currs=" << currs << endl << endl;
    if (currs<prevs) {
      if (DEBUG) cerr << "currs<prevs" << endl;
      for (int s = currs; s <= prevs; s++) {
        if (sfw.find(f[s])!=sfw.end()) {
          WordID target = F2EProjectionFromExternal(s,rule.a_,"_SEP_");
          if (DEBUG) cerr<<"  f[s]="<<TD::Convert(f[s])<<" is a function word, target="<<TD::Convert(target)<<endl;
          //*cost += ScoreDominance(table,1,kSOS,f[s],kSOS,target) + ScoreDominance(table,2,f[s],kEOS,target,kEOS);
          *cost += ScoreDominance(table,1,kSOS,f[s],kUNK,kUNK) + ScoreDominance(table,2,f[s],kEOS,kUNK,kUNK);
          if (DEBUG) cerr << "  resulting cost="<< *cost << endl;
        } else if (f[s]<=0) {
          if (DEBUG) cerr << "  f[s]= is a nonterminal" << endl;
          const int* ants = reinterpret_cast<const int *>(ant_contexts[nt_index[s]-1]);
          *cost += Dwarf::IntegerToDouble(ants[51]); // 50->mono, 51->non-mono 
          if (DEBUG) cerr << "  adding "<< Dwarf::IntegerToDouble(ants[51]) << " into cost, resulting = " << *cost << endl;
        } 
        flag[s] = true;
      }
    }
    prevs = currs;
  } 
  if (DEBUG) cerr << "bonus and state matter" << endl;
  for (int s=0; s<rule.f().size(); s++) {
    if (!flag[s]) {
      if (sfw.find(f[s])!=sfw.end()) {
        WordID target = F2EProjectionFromExternal(s,rule.a_,"_SEP_");
        if (DEBUG) cerr<<"  f[s]="<<TD::Convert(f[s])<<" is a function word, target="<<TD::Convert(target)<<endl;
        //double indbonus = ScoreDominance(table,3,kSOS,f[s],kSOS,target) + ScoreDominance(table,3,f[s],kEOS,target,kEOS);
        double indbonus = ScoreDominance(table,3,kSOS,f[s],kUNK,kUNK) + ScoreDominance(table,3,f[s],kEOS,kUNK,kUNK);
        *bonus += indbonus;
        *state_mono += indbonus;
        //*state_nonmono += ScoreDominance(table,1,kSOS,f[s],kSOS,target) + ScoreDominance(table,2,f[s],kEOS,target,kEOS);
        *state_nonmono += ScoreDominance(table,1,kSOS,f[s],kUNK,kUNK) + ScoreDominance(table,2,f[s],kEOS,kUNK,kUNK);
        if (DEBUG) cerr<<"  new bonus="<<*bonus<<", new state="<<*state_mono<<","<<*state_nonmono<<endl;
      } else if (f[s]<=0) {
        if (DEBUG) cerr << "  f[s]="<< f[s] <<" is a nonterminal" << endl;
        const int* ants = reinterpret_cast<const int *>(ant_contexts[nt_index[s]-1]);
        double indbonus = Dwarf::IntegerToDouble(ants[50]);
        *bonus += indbonus;
        *state_mono += indbonus;
        *state_nonmono += Dwarf::IntegerToDouble(ants[51]);
        if (DEBUG) cerr << "  propagating state=" << *state_mono <<","<< *state_nonmono<< endl;
      }
    }
  }
  if (DEBUG) cerr << "LHS:" << rule.GetLHS() << ":" << TD::Convert(rule.GetLHS()*-1) <<endl;
  if (rule.GetLHS()*-1==TD::Convert("S")) {
    *state_mono = 0;
    *state_nonmono = 0;
    for (int i=0; i<rule.Arity(); i++) { 
      const int* ants = reinterpret_cast<const int *>(ant_contexts[i]);
      *cost += Dwarf::IntegerToDouble(ants[50]);
    }
    *bonus = 0;
  }
  if (DEBUG) cerr << "-->>>> cost="<<*cost<<", bonus="<<*bonus<<", state_mono="<<*state_mono<<", state_nonmono="<<*state_nonmono<<endl;
}

bool Alignment::prepare(TRule& rule, const std::vector<const void*>& ant_contexts, const map<WordID,int>& sfw, const map<WordID,int>& tfw,const Lattice& sourcelattice, int spanstart, int spanend) {  
  if (DEBUG) cerr << "===Rule===" << rule.AsString() << endl;
  _f = rule.f();
  _e = rule.e();
  _Arity = rule.Arity();
  if (DEBUG) {
    cerr << "F: ";
    for (int idx=0; idx<_f.size(); idx++) cerr << _f[idx] << " ";
    cerr << endl;
    cerr << "F': ";
    for (int idx=0; idx<_f.size(); idx++) 
      if (_f[idx]>=0) {
        cerr << TD::Convert(_f[idx]) << " "; 
      } else {
        cerr << TD::Convert(_f[idx]*-1);
      }
    cerr << endl;
    cerr << "E: ";
    for (int idx=0; idx<_e.size(); idx++) 
      cerr << _e[idx] << " ";
    cerr << endl;
    cerr << "E': ";
    for (int idx=0; idx<_e.size(); idx++) 
      if (_e[idx]>0) {
        cerr << TD::Convert(_e[idx]) << " "; 
      } else {
        cerr << "[NT]" << " ";
      }
    cerr << endl;
  }

  SourceFWRuleIdxs[0]=0;
  SourceFWRuleAbsIdxs[0]=0;
  for (int idx=1; idx<=_f.size(); idx++) { // in transformed space
    if (sfw.find(_f[idx-1])!=sfw.end()) {
      SourceFWRuleIdxs[0]++;
      SourceFWRuleAbsIdxs[++SourceFWRuleAbsIdxs[0]]=GetFWGlobalIdx(idx,sourcelattice,_f,spanstart,spanend,ant_contexts,sfw);
      SourceFWRuleIdxs[3*SourceFWRuleIdxs[0]-2]=idx;
      SourceFWRuleIdxs[3*SourceFWRuleIdxs[0]-1]=_f[idx-1];
      SourceFWRuleIdxs[3*SourceFWRuleIdxs[0]]  =F2EProjectionFromExternal(idx-1,rule.a_,"_SEP_");
    }
  }
  TargetFWRuleIdxs[0]=0;
  for (int idx=1; idx<=_e.size(); idx++) { // in transformed space
    if (tfw.find(_e[idx-1])!=tfw.end()) {
      TargetFWRuleIdxs[0]++;
      TargetFWRuleIdxs[3*TargetFWRuleIdxs[0]-2]=idx;
      TargetFWRuleIdxs[3*TargetFWRuleIdxs[0]-1]=E2FProjectionFromExternal(idx-1,rule.a_,"_SEP_");
      TargetFWRuleIdxs[3*TargetFWRuleIdxs[0]]  =_e[idx-1];
    }
  }

  if (DEBUG) {
    cerr << "SourceFWRuleIdxs[" << SourceFWRuleIdxs[0] << "]:";
    for (int idx=1; idx<=SourceFWRuleIdxs[0]; idx++) {
      cerr << " idx:" << SourceFWRuleIdxs[3*idx-2];
      cerr << " absidx:" << SourceFWRuleAbsIdxs[idx];
      cerr << " F:" << SourceFWRuleIdxs[3*idx-1];
      cerr << " E:" << SourceFWRuleIdxs[3*idx];
      cerr << "; ";
    }
    cerr << endl;
    cerr << "TargetFWRuleIdxs[" << TargetFWRuleIdxs[0] << "]:";
    for (int idx=1; idx<=TargetFWRuleIdxs[0]; idx++) {
      cerr << " idx:" << TargetFWRuleIdxs[3*idx-2];
      cerr << " F:" << TargetFWRuleIdxs[3*idx-1];
      cerr << " E:" << TargetFWRuleIdxs[3*idx];
    }
    cerr << endl;
  }
  if (SourceFWRuleIdxs[0]+TargetFWRuleIdxs[0]==0) {
    bool nofw = true;
    for (int i_ant=0; i_ant<_Arity && nofw; i_ant++) { 
      const int* ants = reinterpret_cast<const int *>(ant_contexts[i_ant]);
      if (ants[0]>=0||ants[3]>=0||ants[6]>=0||ants[9]>=0) nofw=false;
    } 
    if (nofw) return true;
  }
  //cerr << "clearing als first" << endl;
  clearAls(_J,_I);

  if (DEBUG) cerr << "A["<< rule.a_.size() << "]: " ;
  RuleAl[0]=0;
  // add phrase start boundary
  RuleAl[0]++; RuleAl[RuleAl[0]*2-1]=0; RuleAl[RuleAl[0]*2]=0;
  if (DEBUG) cerr << RuleAl[RuleAl[0]*2-1] << "-" << RuleAl[RuleAl[0]*2] << " ";
  for (int idx=0; idx<rule.a_.size(); idx++) {
    RuleAl[0]++;
    RuleAl[RuleAl[0]*2-1]=rule.a_[idx].s_+1;
    RuleAl[RuleAl[0]*2]  =rule.a_[idx].t_+1;
    if (DEBUG) cerr << RuleAl[RuleAl[0]*2-1] << "-" << RuleAl[RuleAl[0]*2] << " ";
  }
  // add phrase end boundary
  RuleAl[0]++; RuleAl[RuleAl[0]*2-1]=_f.size()+1; RuleAl[RuleAl[0]*2]=_e.size()+1;
  if (DEBUG) cerr << RuleAl[RuleAl[0]*2-1] << "-" << RuleAl[RuleAl[0]*2] << " ";
  if (DEBUG) cerr << endl;

  SourceRuleIdxs[0] = _f.size()+2; // +2 (phrase boundaries)
  TargetRuleIdxs[0] = _e.size()+2; 
  int ntidx=-1;
  for (int idx=0; idx<_f.size()+2; idx++) { // idx in transformed space
    SourceRuleIdxs[idx+1]=idx;
    if (0<idx && idx<=_f.size()) if (_f[idx-1]<0) SourceRuleIdxs[idx+1]=ntidx--;
  }
  for (int idx=0; idx<_e.size()+2; idx++) {
    TargetRuleIdxs[idx+1]=idx;
    if (0<idx && idx<=_e.size()) {
      //cerr << "_e[" <<(idx-1)<< "]=" << _e[idx-1] << endl;
      if (_e[idx-1]<=0) TargetRuleIdxs[idx+1]=_e[idx-1]-1;
    }
  }
  if (DEBUG) {
    cerr << "SourceRuleIdxs:";
    for (int idx=0; idx<SourceRuleIdxs[0]+1; idx++) 
      cerr << " " << SourceRuleIdxs[idx];
    cerr << endl;
    cerr << "TargetRuleIdxs:";
    for (int idx=0; idx<TargetRuleIdxs[0]+1; idx++) 
      cerr << " " << TargetRuleIdxs[idx];
    cerr << endl;
  }

  // sloppy, the integrity of anstates is assumed
  // total = 50 bytes
  // first 3 ints for leftmost source function words (1 for index, 4 for source WordID and 4 for target WordI
  // second 3 for rightmost source function words
  // third 3 for leftmost target function words
  // fourth 3 for rightmost target function words
  // the next 1 int for the number of alignments
  // the remaining 37 ints for alignments (source then target)
  for (int i_ant=0; i_ant<_Arity; i_ant++) {
    const int* ants = reinterpret_cast<const int *>(ant_contexts[i_ant]);
    int span = ants[Dwarf::STATE_SIZE-1];
    if (DEBUG) {
      cerr << "antcontexts[" << i_ant << "] ";
      for (int idx=0; idx<Dwarf::STATE_SIZE; idx++) cerr << idx << "." << ants[idx] << " ";
      cerr << endl;
      cerr << "i,j = " << source(ants[Dwarf::STATE_SIZE-1]) << "," << target(ants[Dwarf::STATE_SIZE-1]) << endl;
    }
    SourceFWAntsIdxs[i_ant][0]=0;
    SourceFWAntsAbsIdxs[i_ant][0]=0;
    if (ants[0]>=0) {
      // Given a span, give the index of the first function word
      int firstfwidx = GetFirstFWIdx(source(span),target(span),sourcelattice,sfw);
      if (DEBUG) cerr << "  firstfwidx = " << firstfwidx << endl;
      int fwcount = 0;
      if (ants[1]>=0) { // one function word
        SourceFWAntsIdxs[i_ant][0]++; SourceFWAntsIdxs[i_ant][1]=ants[0];
        SourceFWAntsIdxs[i_ant][2]=ants[1]; SourceFWAntsIdxs[i_ant][3]=ants[2]; 
        fwcount++;
      } else { // if ants[1] < 0 then compound fws
        //cerr << "ants[1]<0" << endl;
        istringstream ossf(TD::Convert(ants[1]*-1)); string ffw;
        istringstream osse(TD::Convert(ants[2])); string efw; //projection would be mostly NULL
        int delta=ants[0];
        while (osse >> efw && ossf >> ffw) {
          SourceFWAntsIdxs[i_ant][0]++; 
          SourceFWAntsIdxs[i_ant][SourceFWAntsIdxs[i_ant][0]*3-2]=ants[0]-(delta--);
          SourceFWAntsIdxs[i_ant][SourceFWAntsIdxs[i_ant][0]*3-1]=TD::Convert(ffw);  
          SourceFWAntsIdxs[i_ant][SourceFWAntsIdxs[i_ant][0]*3]  =TD::Convert(efw);
          fwcount++;
        }
      }
      if (DEBUG) cerr << " fwcount=" << fwcount << endl;
      SourceFWAntsAbsIdxs[i_ant][0]=fwcount;
      for (int i=1; i<=fwcount; i++) SourceFWAntsAbsIdxs[i_ant][i]=firstfwidx++;
    }
    if (ants[3]>=0) {
      int lastfwidx = GetLastFWIdx(source(span),target(span),sourcelattice,sfw);
      if (DEBUG) cerr << "  lastfwidx = " << lastfwidx << endl;
      int fwcount=0;
      if (ants[4]>=0) {
        fwcount++;
        SourceFWAntsIdxs[i_ant][0]++; 
        SourceFWAntsIdxs[i_ant][SourceFWAntsIdxs[i_ant][0]*3-2]=ants[3]; 
        SourceFWAntsIdxs[i_ant][SourceFWAntsIdxs[i_ant][0]*3-1]=ants[4]; 
        SourceFWAntsIdxs[i_ant][SourceFWAntsIdxs[i_ant][0]*3]  =ants[5];
      } else { // if ants[4] < 0 then compound fws
        //cerr << "ants[4]<0" << endl;
        istringstream ossf(TD::Convert(ants[4]*-1)); string ffw;
        istringstream osse(TD::Convert(ants[5]));    string efw;
        int delta=0;
        while (osse >> efw && ossf >> ffw) {
          fwcount++;
          SourceFWAntsIdxs[i_ant][0]++;
          SourceFWAntsIdxs[i_ant][SourceFWAntsIdxs[i_ant][0]*3-2]=ants[3]+(delta++);
          SourceFWAntsIdxs[i_ant][SourceFWAntsIdxs[i_ant][0]*3-1]=TD::Convert(ffw);
          SourceFWAntsIdxs[i_ant][SourceFWAntsIdxs[i_ant][0]*3]  =TD::Convert(efw);
        }
      }
      if (DEBUG) cerr << " fwcount=" << fwcount << endl;
      for (int i=1; i<=fwcount; i++) SourceFWAntsAbsIdxs[i_ant][SourceFWAntsAbsIdxs[i_ant][0]+i]=lastfwidx-fwcount+i;
      SourceFWAntsAbsIdxs[i_ant][0]+=fwcount;
    }
    TargetFWAntsIdxs[i_ant][0]=0;
    if (ants[6]>=0) {
      if (ants[8]>=0) { // check the e part 
        TargetFWAntsIdxs[i_ant][0]++; 
        TargetFWAntsIdxs[i_ant][1]=ants[6];
        TargetFWAntsIdxs[i_ant][2]=ants[7]; 
        TargetFWAntsIdxs[i_ant][3]=ants[8];
      } else { // if ants[8] < 0 then compound fws
        //cerr << "ants[8]<0" << endl;
        //cerr << "ants[7]=" << TD::Convert(ants[7]) << endl;
        //cerr << "ants[8]=" << TD::Convert(ants[8]*-1) << endl;
        istringstream ossf(TD::Convert(ants[7]));    string ffw;
        istringstream osse(TD::Convert(ants[8]*-1)); string efw;
        int delta=ants[6];
        while (osse >> efw && ossf >> ffw) {
          //cerr << "efw="<< efw << ",ffw=" << ffw << endl;
          TargetFWAntsIdxs[i_ant][0]++;
          TargetFWAntsIdxs[i_ant][TargetFWAntsIdxs[i_ant][0]*3-2]=ants[6]-(delta--);
          TargetFWAntsIdxs[i_ant][TargetFWAntsIdxs[i_ant][0]*3-1]=TD::Convert(ffw);
          TargetFWAntsIdxs[i_ant][TargetFWAntsIdxs[i_ant][0]*3]  =TD::Convert(efw);
        }
      }
    }
    if (ants[9]>=0) {
      if (ants[11]>=0) {
        TargetFWAntsIdxs[i_ant][0]++; 
        TargetFWAntsIdxs[i_ant][TargetFWAntsIdxs[i_ant][0]*3-2]=ants[9];
        TargetFWAntsIdxs[i_ant][TargetFWAntsIdxs[i_ant][0]*3-1]=ants[10];
        TargetFWAntsIdxs[i_ant][TargetFWAntsIdxs[i_ant][0]*3]  =ants[11];
      } else {
        //cerr << "ants[11]<0" << endl;
        //cerr << "ants[10]=" << TD::Convert(ants[10]) << endl;
        //cerr << "ants[11]=" << TD::Convert(ants[11]*-1) << endl;
        istringstream ossf(TD::Convert(ants[10]));    string ffw;
        istringstream osse(TD::Convert(ants[11]*-1)); string efw;
        int delta = 0;
        while (osse >> efw && ossf >> ffw) {
          //cerr << "efw="<< efw << ",ffw=" << ffw << endl;
          TargetFWAntsIdxs[i_ant][0]++;
          TargetFWAntsIdxs[i_ant][TargetFWAntsIdxs[i_ant][0]*3-2]=ants[9]+(delta++);
          TargetFWAntsIdxs[i_ant][TargetFWAntsIdxs[i_ant][0]*3-1]=TD::Convert(ffw);
          TargetFWAntsIdxs[i_ant][TargetFWAntsIdxs[i_ant][0]*3]  =TD::Convert(efw);
        }
      }
    }
    AntsAl[i_ant][0]=ants[12];//number of alignments
    for (int idx=1; idx<=AntsAl[i_ant][0]; idx++) {
      AntsAl[i_ant][idx*2-1] = source(ants[12+idx]);
      AntsAl[i_ant][idx*2]   = target(ants[12+idx]);
    }
  }

  for (int i_ant=0; i_ant<_Arity; i_ant++) {
    int length = AntsAl[i_ant][0];
    int maxs = -1000;
    int maxt = -1000;
    for (int idx=0; idx<length; idx++) {
      if (maxs<AntsAl[i_ant][2*idx+1]) maxs = AntsAl[i_ant][2*idx+1];
      if (maxt<AntsAl[i_ant][2*idx+2]) maxt = AntsAl[i_ant][2*idx+2];
    }
    if (DEBUG) cerr << "SourceFWAntsIdxs[" <<i_ant<<"][0]=" << SourceFWAntsIdxs[i_ant][0] << endl;
    for (int idx=1; idx<=SourceFWAntsIdxs[i_ant][0]; idx++) {
      if (DEBUG) {
        cerr << "SourceFWAntsIdxs["<<i_ant<<"]["<<(3*idx-2)<<"]="<<SourceFWAntsIdxs[i_ant][3*idx-2];
        cerr << ","<<SourceFWAntsIdxs[i_ant][3*idx-1]<<","<<SourceFWAntsIdxs[i_ant][3*idx]<<endl;
        cerr << "SourceFWAntsAbsIdxs["<<i_ant<<"]["<<idx<<"]="<<SourceFWAntsAbsIdxs[i_ant][idx] << endl;
      }
      if (maxs<SourceFWAntsIdxs[i_ant][3*idx-2]) maxs=SourceFWAntsIdxs[i_ant][3*idx-2];
    }
    if (DEBUG) cerr << "TargetFWAntsIdxs[" <<i_ant<<"][0]=" << TargetFWAntsIdxs[i_ant][0] << endl;
    for (int idx=1; idx<=TargetFWAntsIdxs[i_ant][0]; idx++) {
      if (DEBUG) {
        cerr << "TargetFWAntsIdxs["<<i_ant<<"]["<<(3*idx-2)<<"]="<<TargetFWAntsIdxs[i_ant][3*idx-2];
        cerr << ","<<TargetFWAntsIdxs[i_ant][3*idx-1]<<","<<TargetFWAntsIdxs[i_ant][3*idx]<<endl;
      }
      if (maxt<TargetFWAntsIdxs[i_ant][3*idx-2]) maxt=TargetFWAntsIdxs[i_ant][3*idx-2];
    }
    SourceAntsIdxs[i_ant][0] = maxs+1;
    if (DEBUG) cerr << "SourceAntsIdxs[" << i_ant << "][0]=" <<SourceAntsIdxs[i_ant][0] << endl;
    for (int idx=0; idx<SourceAntsIdxs[i_ant][0]; idx++) SourceAntsIdxs[i_ant][idx+1]=idx;
    TargetAntsIdxs[i_ant][0] = maxt+1;
    if (DEBUG) cerr << "TargetAntsIdxs[" << i_ant << "][0]=" <<TargetAntsIdxs[i_ant][0] << endl;
    for (int idx=0; idx<TargetAntsIdxs[i_ant][0]; idx++) TargetAntsIdxs[i_ant][idx+1]=idx;
  }
  int TotalSource = SourceRuleIdxs[0] - _Arity;
  for (int idx=0; idx<_Arity; idx++) TotalSource += SourceAntsIdxs[idx][0];
  int TotalTarget = TargetRuleIdxs[0] - _Arity;
  for (int idx=0; idx<_Arity; idx++) TotalTarget += TargetAntsIdxs[idx][0];
  if (DEBUG) cerr << "TotalSource = "<< TotalSource << ", TotalTarget = "<< TotalTarget << endl;
  int curr = 0;
  for (int idx=1; idx<=SourceRuleIdxs[0]; idx++) {
    if (SourceRuleIdxs[idx]>=0) {
      SourceRuleIdxs[idx]=curr++;
    } else {
      int i_ant = SourceRuleIdxs[idx]*-1-1;
      if (DEBUG) cerr << "SourceAntsIdxs[" << i_ant << "]" << endl;
      for (int idx2=1; idx2<=SourceAntsIdxs[i_ant][0]; idx2++) {
        SourceAntsIdxs[i_ant][idx2]=curr++;
        if (DEBUG) cerr << SourceAntsIdxs[i_ant][idx2] << " ";
      }
      if (DEBUG) cerr << endl;
    }
  }
  if (DEBUG) {
    cerr << "SourceRuleIdxs" << endl;
    for (int idx=1; idx<=SourceRuleIdxs[0]; idx++) cerr << SourceRuleIdxs[idx] << " ";
    cerr << endl;
  }
  curr = 0;
  for (int idx=1; idx<=TargetRuleIdxs[0]; idx++) {
    if (TargetRuleIdxs[idx]>=0) {
      TargetRuleIdxs[idx]=curr++;
    } else {
      int i_ant = TargetRuleIdxs[idx]*-1-1;
      if (DEBUG) cerr << "TargetRuleIdxs[" << i_ant << "]" << endl;
      for (int idx2=1; idx2<=TargetAntsIdxs[i_ant][0]; idx2++) {
        TargetAntsIdxs[i_ant][idx2]=curr++;
        if (DEBUG) cerr << TargetAntsIdxs[i_ant][idx2] << " ";
      }
      if (DEBUG) cerr << endl;
    }
  }
  if (DEBUG) {
    cerr << "TargetRuleIdxs" << endl;
    for (int idx=1; idx<=TargetRuleIdxs[0]; idx++) cerr << TargetRuleIdxs[idx] << " ";
    cerr << endl;
  }
  for (int idx=1; idx<=RuleAl[0]; idx++) {
    if (DEBUG) {
      cerr << RuleAl[idx*2-1] << " - " << RuleAl[idx*2] << " to ";
      cerr << SourceRuleIdxs[RuleAl[idx*2-1]+1] << " - " << TargetRuleIdxs[RuleAl[idx*2]+1] << endl;
    }
    set(SourceRuleIdxs[RuleAl[idx*2-1]+1],TargetRuleIdxs[RuleAl[idx*2]+1]);
  }
  for (int i_ant=0; i_ant<_Arity; i_ant++) {
    for (int idx=1; idx<=AntsAl[i_ant][0]; idx++) {
      if (DEBUG) {
        cerr << AntsAl[i_ant][2*idx-1] << " - " << AntsAl[i_ant][2*idx] << " to ";
        cerr << SourceAntsIdxs[i_ant][AntsAl[i_ant][2*idx-1]+1] << " - ";
        cerr << TargetAntsIdxs[i_ant][AntsAl[i_ant][2*idx]+1] << endl;
      }
      set(SourceAntsIdxs[i_ant][AntsAl[i_ant][2*idx-1]+1],TargetAntsIdxs[i_ant][AntsAl[i_ant][2*idx]+1]);
    }
  }
  SourceFWIdxs[0]=0;
  SourceFWAbsIdxs[0]=0;
  if (DEBUG) cerr << "SourceFWRuleIdxs:" << endl;
  for (int idx=1; idx<=SourceFWRuleIdxs[0]; idx++) {
    if (DEBUG) cerr << SourceFWRuleIdxs[3*idx-2] << " to " << SourceRuleIdxs[SourceFWRuleIdxs[3*idx-2]+1] << endl;
    SourceFWRuleIdxs[3*idx-2] = SourceRuleIdxs[SourceFWRuleIdxs[3*idx-2]+1];
    SourceFWAbsIdxs[0]++;
    SourceFWAbsIdxs[3*SourceFWAbsIdxs[0]-2]=SourceFWRuleAbsIdxs[idx];
    SourceFWIdxs[0]++;
    SourceFWIdxs[3*SourceFWIdxs[0]-2]=SourceFWRuleIdxs[3*idx-2];
    SourceFWIdxs[3*SourceFWIdxs[0]-1]=SourceFWRuleIdxs[3*idx-1];
    SourceFWIdxs[3*SourceFWIdxs[0]]  =SourceFWRuleIdxs[3*idx];
  }
  for (int i_ant=0; i_ant<_Arity; i_ant++) {
    if (DEBUG) cerr << "SourceFWAntsIdxs[" << i_ant << "]" << endl;
    for (int idx=1; idx<=SourceFWAntsIdxs[i_ant][0]; idx++) {
      if (DEBUG) 
        cerr << SourceFWAntsIdxs[i_ant][3*idx-2] << " to " << SourceAntsIdxs[i_ant][SourceFWAntsIdxs[i_ant][3*idx-2]+1] << endl;
      SourceFWAntsIdxs[i_ant][3*idx-2] = SourceAntsIdxs[i_ant][SourceFWAntsIdxs[i_ant][3*idx-2]+1];
      SourceFWAbsIdxs[0]++;
      SourceFWAbsIdxs[3*SourceFWAbsIdxs[0]-2]=SourceFWAntsAbsIdxs[i_ant][idx];
      SourceFWIdxs[0]++;
      SourceFWIdxs[3*SourceFWIdxs[0]-2]=SourceFWAntsIdxs[i_ant][3*idx-2];
      SourceFWIdxs[3*SourceFWIdxs[0]-1]=SourceFWAntsIdxs[i_ant][3*idx-1];
      SourceFWIdxs[3*SourceFWIdxs[0]]  =SourceFWAntsIdxs[i_ant][3*idx];
    }
  }
  sort(SourceFWIdxs);
  sort(SourceFWAbsIdxs);
  if (DEBUG) {
    cerr << "SourceFWIdxs : ";
    for (int idx=1; idx<=SourceFWIdxs[0]; idx++) {
      cerr << "idx:" << SourceFWIdxs[3*idx-2] << ",";
      cerr << "F:" << SourceFWIdxs[3*idx-1] << ",";
      cerr << "E:" << SourceFWIdxs[3*idx] << " ";
    }
    cerr << endl;
  }
  TargetFWIdxs[0]=0;
  if (DEBUG) cerr << "TargetFWRuleIdxs:" << endl;
  for (int idx=1; idx<=TargetFWRuleIdxs[0]; idx++) {
    if (DEBUG) cerr << TargetFWRuleIdxs[3*idx-2] << " to " << TargetRuleIdxs[TargetFWRuleIdxs[3*idx-2]+1] << endl;
    TargetFWRuleIdxs[3*idx-2] = TargetRuleIdxs[TargetFWRuleIdxs[3*idx-2]+1];
    TargetFWIdxs[0]++;    
    TargetFWIdxs[3*TargetFWIdxs[0]-2]=TargetFWRuleIdxs[3*idx-2];
    TargetFWIdxs[3*TargetFWIdxs[0]-1]=TargetFWRuleIdxs[3*idx-1];
    TargetFWIdxs[3*TargetFWIdxs[0]]  =TargetFWRuleIdxs[3*idx];
  }
  for (int i_ant=0; i_ant<_Arity; i_ant++) {
    if (DEBUG) cerr << "TargetFWAntsIdxs[" << i_ant << "]" << endl;
    for (int idx=1; idx<=TargetFWAntsIdxs[i_ant][0]; idx++) {
      if (DEBUG) cerr << TargetFWAntsIdxs[i_ant][3*idx-2] << " to " << TargetAntsIdxs[i_ant][TargetFWAntsIdxs[i_ant][3*idx-2]+1] << endl;
      TargetFWAntsIdxs[i_ant][3*idx-2] = TargetAntsIdxs[i_ant][TargetFWAntsIdxs[i_ant][3*idx-2]+1];
      TargetFWIdxs[0]++;
      TargetFWIdxs[3*TargetFWIdxs[0]-2]=TargetFWAntsIdxs[i_ant][3*idx-2];
      TargetFWIdxs[3*TargetFWIdxs[0]-1]=TargetFWAntsIdxs[i_ant][3*idx-1];
      TargetFWIdxs[3*TargetFWIdxs[0]]  =TargetFWAntsIdxs[i_ant][3*idx];
    }
  }
  sort(TargetFWIdxs);
  if (DEBUG) {
    cerr << "TargetFWIdxs : ";
    for (int idx=1; idx<=TargetFWIdxs[0]; idx++) {
      cerr << "idx:" << TargetFWIdxs[3*idx-2]<< ",";
      cerr << "E:" << TargetFWIdxs[3*idx-1]<< ",";
      cerr << "F:" << TargetFWIdxs[3*idx]<< " ";
    }
    cerr << endl;
    cerr << AsString() << endl;
  }
  fas = firstSourceAligned(1); las = lastSourceAligned(_J-2);
  fat = firstTargetAligned(1); lat = lastTargetAligned(_I-2); 
  if (DEBUG) cerr << "fas=" << fas << ", las=" << las << ", fat=" << fat << ", lat=" << lat << endl;
  assert(fas<=las);
  assert(fat<=lat);
  SetCurrAlVector();
  if (DEBUG) cerr << "end prepare" << endl;
  return false;
}      

string Alignment::AsStringSimple() {
  ostringstream stream;
  for (int j=0; j<getJ(); j++) {
    int t = targetOf(j,minTSpan(j));
    while (t>=0) {
      stream << " " << j << "-" << t;
      t = targetOf(j,t+1);
    }
  }
  return stream.str();
};


string Alignment::AsString() {
  ostringstream stream;
  stream << "J:" << getJ() << " I:" << getI();
  for (int j=0; j<getJ(); j++) {
    int t = targetOf(j,minTSpan(j));
    while (t>=0) {
      stream << " " << j << "-" << t;
      t = targetOf(j,t+1);
    }
  }
  stream << " TargetSpan:";
  for (int j=0; j<getJ(); j++)
    if (minTSpan(j)!=MINIMUM_INIT)
      stream << " " << j << "[" << minTSpan(j) << "," << maxTSpan(j) << "]";
    else
      stream << " " << j << "[-,-]";
  stream << " SourceSpan:";
  for (int i=0; i<getI(); i++)
    if (minSSpan(i)!=MINIMUM_INIT)
      stream << " " << i << "[" << minSSpan(i) << "," << maxSSpan(i) << "]";
    else
      stream << " " << i << "[-,-]";
  return stream.str();
};

void Alignment::SetCurrAlVector() {
  curr_al.clear();
  for (int j=0; j<_J; j++) {
    int i = targetOf(j);
    while (i>=0) {
      curr_al.push_back(link(j,i));
      i = targetOf(j,i+1);
    }
  }
}

void CountTable::print() const {
  cerr << "+++ Model +++" << endl;
  for (map<WordID,int*>::const_iterator iter=model.begin(); iter!=model.end(); iter++) {
    cerr << TD::Convert(iter->first) << " ";
    for (int i=0; i<numColumn; i++) cerr << iter->second[i] << " ";
    cerr << endl;
  }
  cerr << "+++ Ultimate +++" << endl;
  for (int i=0; i<numColumn; i++) cerr << ultimate[i] << " ";
  cerr << endl;
}

void Alignment::ToArrayInt(vector<int>* ret) {
  ret->clear();
  for (int i=0; i<_J; i++) {
    int t = targetOf(i);
    while (t>=0) {
      ret->push_back(link(i,t));
      t = targetOf(i,t+1);
    }
  }
}

int Alignment::GetFWGlobalIdx(int idx, const Lattice& sourcelattice, vector<WordID>& sources, int spanstart, int spanend, const std::vector<const void*>& ant_contexts, const map<WordID,int>& sfw) {
  // get the index of the function word in the lattice
  if (DEBUG) cerr << "   GetFWGlobalIdx(" << idx << "," << spanstart << "," << spanend << ")" << endl;
  int curr = spanstart; int i_ant = 0;
  for (int i=1; i<sources.size() && i<idx; i++) { // sources contain <s> and </s> 
    if (sources[i]<0) {
      const int* ants = reinterpret_cast<const int *>(ant_contexts[i_ant++]);
      int antstate = ants[Dwarf::STATE_SIZE-1];
      if (DEBUG) cerr << "    found NT[" << target(antstate) << "," << source(antstate) << "]" << endl;
      curr += target(antstate)-source(antstate);
    } else {
      curr++;
    }
  }
  if (DEBUG) cerr << "    curr = " << curr << endl;
  //compute the fw index
  int ret = 1;
  for (int i=0; i<curr; i++) {
    if (sfw.find(sourcelattice[i][0].label)!=sfw.end()) ret++;
  }
  if (DEBUG) cerr << "    ret = " << ret << endl;
  return ret;
}

int Alignment::GetFirstFWIdx(int spanstart,int spanend, const Lattice& sourcelattice, const map<WordID,int>& sfw) {
  if (DEBUG) cerr << "   GetFirstFWIdx(" << spanstart << "," << spanend << ")" << endl;  
  int curr=0;
  for (int i=0; i<spanend; i++) {
    if (sfw.find(sourcelattice[i][0].label)!=sfw.end()) {
      curr++;
      if (i>=spanstart) return curr;
    } 
  }
//  assert(0);
  return curr;
}

int Alignment::GetLastFWIdx(int spanstart,int spanend, const Lattice& sourcelattice, const map<WordID,int>& sfw) {
  if (DEBUG) cerr << "   GetLastFWIdx(" << spanstart << "," << spanend << ")" << endl;
  int curr=0;
  for (int i=0; i<spanend; i++) {
    if (sfw.find(sourcelattice[i][0].label)!=sfw.end()) {
      curr++;
    } 
  }
  return curr;
}

WordID Alignment::generalize(WordID original, const map<WordID,WordID>& tags, bool pos) {
  if (!pos) {
    map<WordID,WordID>::const_iterator it = tags.find(original);
    if (it!=tags.end()) {
      return it->second;
    }
  } else {
    string key,idx;
    Dwarf::stripIndex(TD::Convert(original),&key,&idx);
    map<WordID,WordID>::const_iterator it = tags.find(TD::Convert(key)); 
    if (it!=tags.end()) {
      ostringstream oss;
      oss << TD::Convert(it->second) << "/" << idx;
      return TD::Convert(oss.str());   
    }
  }
  return original;
}

int* Alignment::SOS() {
  int* neighbor = new int[4];
  neighbor[0]=0; neighbor[1]=0;
  neighbor[2]=0; neighbor[3]=0;
  return neighbor;
}

int* Alignment::EOS() {
  int* neighbor = new int[4];
  neighbor[0]=getJ()-1; neighbor[1]=neighbor[0];
  neighbor[2]=getI()-1; neighbor[3]=neighbor[2];
  return neighbor;
}

int* Alignment::neighborLeft(int startidx, int endidx, bool* getit) {
  if (DEBUG) cerr << "   neighborLeft("<<startidx<<","<<endidx<<")"<<endl;
  int lborder = startidx;
  int* ret;
  while(lborder<=endidx) {
    ret = blockSource(lborder,endidx);
    if (ret[0]==lborder && ret[1]==endidx && ret[2]!=MINIMUM_INIT) {
      *getit = true;
      return ret;
    } else {
      delete[] ret;
      lborder++;
    }
  }
  ret = new int[4];
  ret[0]=-1; ret[1]=-1; ret[2]=-1; ret[3]=-1;
  *getit = false;
  return ret;
}

int* Alignment:: neighborRight(int startidx, int endidx, bool* getit) {
  if (DEBUG) cerr << "   neighborRight("<<startidx<<","<<endidx<<")"<<endl;
  int rborder = endidx;
  int* ret;
  while(startidx<=rborder) {
    ret = blockSource(startidx,rborder);
    if (ret[0]==startidx && ret[1]==rborder && ret[2]!=MINIMUM_INIT) {
      *getit = true;
      return ret;
    } else {
      delete[] ret;
      rborder--;
    }
  }
  ret = new int[4];
  ret[0]=-1; ret[1]=-1; ret[2]=-1; ret[3]=-1;
  *getit = false;
  return ret;
}
