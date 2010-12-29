#include <vector>
#include <map>
#include <string>
#include "ff.h"
#include "dwarf.h"
#include "lattice.h"

using namespace std;

class Dwarf : public FeatureFunction {
 public:
  Dwarf(const std::string& param);
  /* State-related param
     STATE_SIZE: the number of ints 
     MAXIMUM_ALIGNMENTS: the maximum number of alignments in the states, 
                         each alignment point is encoded in one int 
                         (the first two bytes for source, and the remaining one for target)
  */
  static const int STATE_SIZE=53; 
  static const int IMPOSSIBLY_LARGE_POS = 9999999;
  static const int MAXIMUM_ALIGNMENTS=37;
  /* Read from file the Orientation(Source|Target model parameter. */ 
  static bool readOrientation(CountTable* table, const std::string& filename, std::map<WordID,int> *fw, bool pos=false);
  /* Read from file the Dominance(Source|Target) model parameter. */ 
  static bool readDominance(CountTable* table, const std::string& filename, std::map<WordID,int> *fw, bool pos=false);
  static bool readList(const std::string& filename, std::map<WordID,int>* fw);     
  static double IntegerToDouble(int val);
  static int DoubleToInteger(double val);
  bool readTags(const std::string& filename, std::map<WordID,WordID>* tags);
  bool generalizeOrientation(CountTable* table, const std::map<WordID,WordID>& tags, bool pos=false);  
  bool generalizeDominance(CountTable* table, const std::map<WordID,WordID>& tags, bool pos=false);  
  static void stripIndex(const string& source, string* pkey, string* pidx) {
    if (DEBUG) cerr << "    stripIndex(" << source << ")" << endl;
    int found = source.find_last_of("/");
    string idx = source.substr(found+1);
    string key = source.substr(0,found);
    if (DEBUG) cerr << "      found=" << found << "," << key << "," << idx << endl;
    pkey = &key;
    pidx = &idx;
  }


 protected:
  /* The high-level workflow is as follow:
     1. call *als->prepare*, which constructs the full alignment of the edge while taking into account the antecedents
        also in this call, function words are identified. Most of the work in this call is to make sure the indexes 
        of the alignments (including the function words) are consistent with the newly created alignment
     2. call *als->computeOrientationSource*, *als->computeOrientationTarget*, 
        *als->computeDominanceSource*, or *als->computeDominanceTarget*
        and pass the resulting score to either *features* or to *estimated_features*
     3. call *als->BorderingSFWsOnly()* and *als->BorderingTFWsOnly()*, which removes records of all function word
        alignments except those at the borders. Note that fw alignments kept may be more than two on each side
        for examples if there are a number of unaligned fw alignments before the leftmost alignment or the rightmost one
     4. call *als->simplify()*, which assigns the state of this edge (*context*). It simplifies the alignment space to 
        its most compact representation, enough to compute the unscored models. This is done by observing the surviving
        function word alignments set by 3.
  */ 
  void TraversalFeaturesImpl(const SentenceMetadata& smeta,
                                     const Hypergraph::Edge& edge,
                                     const std::vector<const void*>& ant_contexts,
                                     SparseVector<double>* features,
                                     SparseVector<double>* estimated_features,
                                     void* context) const;
 private:
  Alignment* als;
  /* Feature IDs set by calling FD::Convert(model's string) */
  int oris_, oris_bo1_, oris_bo2_, orit_, orit_bo1_, orit_bo2_;
  int oris_backward_, orit_backward_, porislr_, porisrl_, goris_, pgorislr_, pgorisrl_;
  int pdomslr_, pdomsrl_, pgdomslr_, pgdomsrl_;
  int doms_, doms_bo1_, doms_bo2_, domt_, domt_bo1_, domt_bo2_;
  int tfw_count_;
  int bdoms_; 
  int poris_count;
  int pgoris_count;
  int poris_nlr, poris_nrl; // maximum depth (1->from the beginning of the sentence, 2-> from the end of the sentence)
  int pgoris_nlr, pgoris_nrl;
  int pdoms_nlr, pdoms_nrl;
  int pgdoms_nlr, pgdoms_nrl;
  int* _sent_id;
  int* _fwcount;
  WordID kSOS;
  WordID kEOS;
  string sSOS;
  string sEOS;
  WordID kGOAL;
  /* model's flag, if set true will invoke the model scoring */
  bool flag_oris, flag_orit, flag_doms, flag_domt, flag_tfw_count, flag_oris_backward, flag_orit_backward, flag_bdoms;
  bool flag_porislr, flag_porisrl, flag_goris, flag_pgorislr, flag_pgorisrl;
  bool explicit_soseos;
  bool flag_pdomslr, flag_pdomsrl, flag_pgdomslr, flag_pgdomsrl, flag_gdoms;
  /* a collection of Source function words (sfw) and Target function words (tfw) */
  std::map<WordID,int> sfw;
  std::map<WordID,int> tfw;
  std::map<WordID,WordID> tags;
  /* a collection of model's parameter */
  CountTable toris, torit, tdoms, tbdoms, tdomt, tporislr, tporisrl, tgoris, tpgorislr, tpgorisrl;
  CountTable tpdomslr, tpdomsrl, tpgdomslr, tpgdomsrl;
  void neighboringFWs(const Lattice& l, const int& i, const int& j, const map<WordID,int>& fw_hash, int* lfw, int* rfw);
};

