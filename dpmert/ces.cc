#include "ces.h"

#include <vector>
#include <sstream>
#include <boost/shared_ptr.hpp>

// TODO, if AER is to be optimized again, we will need this
// #include "aligner.h"
#include "lattice.h"
#include "mert_geometry.h"
#include "error_surface.h"
#include "ns.h"

using namespace std;

const bool minimize_segments = true;    // if adjacent segments have equal scores, merge them

void ComputeErrorSurface(const SegmentEvaluator& ss,
                         const ConvexHull& ve,
                         ErrorSurface* env,
                         const EvaluationMetric* metric,
                         const Hypergraph& hg) {
  vector<WordID> prev_trans;
  const vector<boost::shared_ptr<MERTPoint> >& ienv = ve.GetSortedSegs();
  env->resize(ienv.size());
  SufficientStats prev_score; // defaults to 0
  int j = 0;
  for (unsigned i = 0; i < ienv.size(); ++i) {
    const MERTPoint& seg = *ienv[i];
    vector<WordID> trans;
#if 0
    if (type == AER) {
      vector<bool> edges(hg.edges_.size(), false);
      seg.CollectEdgesUsed(&edges);  // get the set of edges in the viterbi
                                     // alignment
      ostringstream os;
      const string* psrc = ss.GetSource();
      if (psrc == NULL) {
        cerr << "AER scoring in VEST requires source, but it is missing!\n";
        abort();
      }
      size_t pos = psrc->rfind(" ||| ");
      if (pos == string::npos) {
        cerr << "Malformed source for AER: expected |||\nINPUT: " << *psrc << endl;
        abort();
      }
      Lattice src;
      Lattice ref;
      LatticeTools::ConvertTextOrPLF(psrc->substr(0, pos), &src);
      LatticeTools::ConvertTextOrPLF(psrc->substr(pos + 5), &ref);
      AlignerTools::WriteAlignment(src, ref, hg, &os, true, 0, &edges);
      string tstr = os.str();
      TD::ConvertSentence(tstr.substr(tstr.rfind(" ||| ") + 5), &trans);
    } else {
#endif
      seg.ConstructTranslation(&trans);
    //}
    //cerr << "Scoring: " << TD::GetString(trans) << endl;
    if (trans == prev_trans) {
      if (!minimize_segments) {
        ErrorSegment& out = (*env)[j];
        out.delta.fields.clear();
        out.x = seg.x;
	++j;
      }
      //cerr << "Identical translation, skipping scoring\n";
    } else {
      SufficientStats score;
      ss.Evaluate(trans, &score);
      // cerr << "score= " << score->ComputeScore() << "\n";
      //string x1; score.Encode(&x1); cerr << "STATS: " << x1 << endl;
      const SufficientStats delta = score - prev_score;
      //string x2; delta.Encode(&x2); cerr << "DELTA: " << x2 << endl;
      //string xx; delta.Encode(&xx); cerr << xx << endl;
      prev_trans.swap(trans);
      prev_score = score;
      if ((!minimize_segments) || (!delta.IsAdditiveIdentity())) {
        ErrorSegment& out = (*env)[j];
        out.delta = delta;
        out.x = seg.x;
        ++j;
      }
    }
  }
  // cerr << " In segments: " << ienv.size() << endl;
  // cerr << "Out segments: " << j << endl;
  assert(j > 0);
  env->resize(j);
}

