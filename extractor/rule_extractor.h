#ifndef _RULE_EXTRACTOR_H_
#define _RULE_EXTRACTOR_H_

#include <memory>

using namespace std;

class Alignment;
class DataArray;
class SuffixArray;

class RuleExtractor {
 public:
  RuleExtractor(
      shared_ptr<SuffixArray> source_suffix_array,
      shared_ptr<DataArray> target_data_array,
      const Alignment& alingment);

  void ExtractRules();
};

#endif
