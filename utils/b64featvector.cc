#include "b64featvector.h"

#include <sstream>
#include <boost/scoped_array.hpp>
#include "b64tools.h"
#include "fdict.h"

using namespace std;

static inline void EncodeFeatureWeight(const string &featname, weight_t weight,
                                       ostream *output) {
  output->write(featname.data(), featname.size() + 1);
  output->write(reinterpret_cast<char *>(&weight), sizeof(weight_t));
}

string EncodeFeatureVector(const SparseVector<weight_t> &vec) {
  string b64;
  {
    ostringstream base64_strm;
    {
      ostringstream strm;
      for (SparseVector<weight_t>::const_iterator it = vec.begin();
           it != vec.end(); ++it)
        if (it->second != 0)
          EncodeFeatureWeight(FD::Convert(it->first), it->second, &strm);
      string data(strm.str());
      B64::b64encode(data.data(), data.size(), &base64_strm);
    }
    b64 = base64_strm.str();
  }
  return b64;
}

void DecodeFeatureVector(const string &data, SparseVector<weight_t> *vec) {
  vec->clear();
  if (data.empty()) return;
  // Decode data
  size_t b64_len = data.size(), len = b64_len / 4 * 3;
  boost::scoped_array<char> buf(new char[len]);
  bool res =
      B64::b64decode(reinterpret_cast<const unsigned char *>(data.data()),
                     b64_len, buf.get(), len);
  assert(res);
  // Apply updates
  size_t cur = 0;
  while (cur < len) {
    string feat_name(buf.get() + cur);
    if (feat_name.empty()) break;  // Encountered trailing \0
    int feat_id = FD::Convert(feat_name);
    weight_t feat_delta =
        *reinterpret_cast<weight_t *>(buf.get() + cur + feat_name.size() + 1);
    (*vec)[feat_id] = feat_delta;
    cur += feat_name.size() + 1 + sizeof(weight_t);
  }
}
