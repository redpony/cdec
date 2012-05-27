#include "sparse_vector.h"

#include <iostream>
#include <cstring>

#include "fdict.h"
#include "b64tools.h"

using namespace std;

namespace B64 {

void Encode(double objective, const SparseVector<double>& v, ostream* out) {
  const int num_feats = v.size();
  size_t tot_size = 0;
  const size_t off_objective = tot_size;
  tot_size += sizeof(double);                   // objective
  const size_t off_num_feats = tot_size;
  tot_size += sizeof(int);                      // num_feats
  const size_t off_data = tot_size;
  tot_size += sizeof(unsigned char) * num_feats; // lengths of feature names;
  typedef SparseVector<double>::const_iterator const_iterator;
  for (const_iterator it = v.begin(); it != v.end(); ++it)
    tot_size += FD::Convert(it->first).size();   // feature names;
  tot_size += sizeof(double) * num_feats;        // gradient
  const size_t off_magic = tot_size; (void) off_magic;
  tot_size += 4;                                 // magic

  // size_t b64_size = tot_size * 4 / 3;
  // cerr << "Sparse vector binary size: " << tot_size << "  (b64 size=" << b64_size << ")\n";
  char* data = new char[tot_size];
  *reinterpret_cast<double*>(&data[off_objective]) = objective;
  *reinterpret_cast<int*>(&data[off_num_feats]) = num_feats;
  char* cur = &data[off_data];
  assert(static_cast<size_t>(cur - data) == off_data);
  for (const_iterator it = v.begin(); it != v.end(); ++it) {
    const string& fname = FD::Convert(it->first);
    *cur++ = static_cast<char>(fname.size());   // name len
    memcpy(cur, &fname[0], fname.size());
    cur += fname.size();
    *reinterpret_cast<double*>(cur) = it->second;
    cur += sizeof(double);
  }
  assert(static_cast<size_t>(cur - data) == off_magic);
  *reinterpret_cast<unsigned int*>(cur) = 0xBAABABBAu;
  cur += sizeof(unsigned int);
  assert(static_cast<size_t>(cur - data) == tot_size);
  b64encode(data, tot_size, out);
  delete[] data;
}

bool Decode(double* objective, SparseVector<double>* v, const char* in, size_t size) {
  v->clear();
  if (size % 4 != 0) {
    cerr << "B64 error - line % 4 != 0\n";
    return false;
  }
  const size_t decoded_size = size * 3 / 4 - sizeof(unsigned int);
  const size_t buf_size = decoded_size + sizeof(unsigned int);
  if (decoded_size < 6) { cerr << "SparseVector decoding error: too short!\n"; return false; }
  char* data = new char[buf_size];
  if (!b64decode(reinterpret_cast<const unsigned char*>(in), size, data, buf_size)) {
    delete[] data;
    return false;
  }
  size_t cur = 0;
  *objective = *reinterpret_cast<double*>(data);
  cur += sizeof(double);
  const int num_feats = *reinterpret_cast<int*>(&data[cur]);
  cur += sizeof(int);
  int fc = 0;
  while(fc < num_feats && cur < decoded_size) {
    ++fc;
    const int fname_len = data[cur++];
    assert(fname_len > 0);
    assert(fname_len < 256);
    string fname(fname_len, '\0');
    memcpy(&fname[0], &data[cur], fname_len);
    cur += fname_len;
    const double val = *reinterpret_cast<double*>(&data[cur]);
    cur += sizeof(double);
    int fid = FD::Convert(fname);
    v->set_value(fid, val);
  }
  if(num_feats != fc) {
    cerr << "Expected " << num_feats << " but only decoded " << fc << "!\n";
    delete[] data;
    return false;
  }
  if (*reinterpret_cast<unsigned int*>(&data[cur]) != 0xBAABABBAu) {
    cerr << "SparseVector decodeding error : magic does not match!\n";
    delete[] data;
    return false;
  }
  delete[] data;
  return true;
}

}
