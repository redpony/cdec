#include "ces.h"

#include <vector>
#include <sstream>
#include <boost/shared_ptr.hpp>

#include "aligner.h"
#include "lattice.h"
#include "viterbi_envelope.h"
#include "error_surface.h"

using boost::shared_ptr;
using namespace std;

const bool minimize_segments = true;    // if adjacent segments have equal scores, merge them

void ComputeErrorSurface(const SentenceScorer& ss, const ViterbiEnvelope& ve, ErrorSurface* env, const ScoreType type, const Hypergraph& hg) {
  vector<WordID> prev_trans;
  const vector<shared_ptr<Segment> >& ienv = ve.GetSortedSegs();
  env->resize(ienv.size());
  ScoreP prev_score;
  int j = 0;
  for (int i = 0; i < ienv.size(); ++i) {
    const Segment& seg = *ienv[i];
    vector<WordID> trans;
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
      seg.ConstructTranslation(&trans);
    }
    // cerr << "Scoring: " << TD::GetString(trans) << endl;
    if (trans == prev_trans) {
      if (!minimize_segments) {
        assert(prev_score); // if this fails, it means
	                    // the decoder can generate null translations
        ErrorSegment& out = (*env)[j];
        out.delta = prev_score->GetZero();
        out.x = seg.x;
	++j;
      }
      // cerr << "Identical translation, skipping scoring\n";
    } else {
      ScoreP score = ss.ScoreCandidate(trans);
      // cerr << "score= " << score->ComputeScore() << "\n";
      ScoreP cur_delta_p = score->GetZero();
      Score* cur_delta = cur_delta_p.get();
      // just record the score diffs
      if (!prev_score)
        prev_score = score->GetZero();

      score->Subtract(*prev_score, cur_delta);
      prev_trans.swap(trans);
      prev_score = score;
      if ((!minimize_segments) || (!cur_delta->IsAdditiveIdentity())) {
        ErrorSegment& out = (*env)[j];
        out.delta = cur_delta_p;
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

