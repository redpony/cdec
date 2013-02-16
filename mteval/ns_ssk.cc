#include "ns_ssk.h"

#include <vector>

#include "kernel_string_subseq.h"
#include "tdict.h"

static const unsigned kNUMFIELDS = 2;
static const unsigned kSIMILARITY = 0;
static const unsigned kCOUNT = 1;

unsigned SSKMetric::SufficientStatisticsVectorSize() const {
  return kNUMFIELDS;
}

void SSKMetric::ComputeSufficientStatistics(const std::vector<WordID>& hyp,
                                            const std::vector<std::vector<WordID> >& refs,
                                            SufficientStats* out) const {
  out->fields.resize(kNUMFIELDS);
  out->fields[kCOUNT] = 1;
  float bestsim = 0;
  for (unsigned i = 0; i < refs.size(); ++i) {
    float s = ssk<4>(hyp, refs[i], 0.8);
    if (s > bestsim) bestsim = s;
  }
  out->fields[kSIMILARITY] = bestsim;
}

float SSKMetric::ComputeScore(const SufficientStats& stats) const {
  return stats.fields[kSIMILARITY] / stats.fields[kCOUNT];
}

