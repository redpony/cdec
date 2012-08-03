#include "ns_cer.h"
#include "tdict.h"

static const unsigned kNUMFIELDS = 2;
static const unsigned kEDITDISTANCE = 0;
static const unsigned kCHARCOUNT = 1;

bool CERMetric::IsErrorMetric() const {
  return true;
}

unsigned CERMetric::SufficientStatisticsVectorSize() const {
  return 2;
}

unsigned CERMetric::EditDistance(const std::string& hyp,
                                 const std::string& ref) const {
  const unsigned m = hyp.size(), n = ref.size();
  std::vector<unsigned> edit((m + 1) * 2);
  for(unsigned i = 0; i < n + 1; i++) {
    for(unsigned j = 0; j < m + 1; j++) {
      if(i == 0)
        edit[j] = j;
      else if(j == 0)
        edit[(i%2)*(m+1)] = i;
      else
        edit[(i%2)*(m+1) + j] = std::min(std::min(edit[(i%2)*(m+1) + j-1] + 1,
                                                   edit[((i-1)%2)*(m+1) + j] + 1),
                                                   edit[((i-1)%2)*(m+1) + (j-1)] 
                                                   + (hyp[j-1] == ref[i-1] ? 0 : 1));
      
    }
  }
  return edit[(n%2)*(m+1) + m];
}

void CERMetric::ComputeSufficientStatistics(const std::vector<WordID>& hyp,
                                            const std::vector<std::vector<WordID> >& refs,
                                            SufficientStats* out) const {
  out->fields.resize(kNUMFIELDS);
  std::string hyp_str(TD::GetString(hyp));
  float best_score = hyp_str.size();
  for (size_t i = 0; i < refs.size(); ++i) {
    std::string ref_str(TD::GetString(refs[i]));
    float score = EditDistance(hyp_str, ref_str);
    if (score < best_score) {
      out->fields[kEDITDISTANCE] = score;
      out->fields[kCHARCOUNT] = ref_str.size();
      best_score = score;
    }
  }
}
float CERMetric::ComputeScore(const SufficientStats& stats) const {
  return stats.fields[kEDITDISTANCE] / stats.fields[kCHARCOUNT];
}
