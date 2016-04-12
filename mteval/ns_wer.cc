#include "ns_wer.h"
#include "tdict.h"
#include "levenshtein.h"

static const unsigned kNUMFIELDS = 2;
static const unsigned kEDITDISTANCE = 0;
static const unsigned kCHARCOUNT = 1;

bool WERMetric::IsErrorMetric() const {
  return true;
}

unsigned WERMetric::SufficientStatisticsVectorSize() const {
  return 2;
}

void WERMetric::ComputeSufficientStatistics(const std::vector<WordID>& hyp,
                                            const std::vector<std::vector<WordID> >& refs,
                                            SufficientStats* out) const {
  out->fields.resize(kNUMFIELDS);
  float best_score = 0;
  for (size_t i = 0; i < refs.size(); ++i) {
    float score = cdec::LevenshteinDistance(hyp, refs[i]);
    if (score < best_score || i == 0) {
      out->fields[kEDITDISTANCE] = score;
      out->fields[kCHARCOUNT] = refs[i].size();
      best_score = score;
    }
  }
}

float WERMetric::ComputeScore(const SufficientStats& stats) const {
  return stats.fields[kEDITDISTANCE] / stats.fields[kCHARCOUNT];
}

