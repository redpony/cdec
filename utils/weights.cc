#include "weights.h"

#include <sstream>

#include "fdict.h"
#include "filelib.h"
#include "verbose.h"

using namespace std;

void Weights::InitFromFile(const std::string& filename, vector<string>* feature_list) {
  if (!SILENT) cerr << "Reading weights from " << filename << endl;
  ReadFile in_file(filename);
  istream& in = *in_file.stream();
  assert(in);
  
  bool read_text = true;
  if (1) {
    ReadFile hdrrf(filename);
    istream& hi = *hdrrf.stream();
    assert(hi);
    char buf[10];
    hi.get(buf, 6);
    assert(hi.good());
    if (strncmp(buf, "_PHWf", 5) == 0) {
      read_text = false;
    }
  }

  if (read_text) {
    int weight_count = 0;
    bool fl = false;
    string buf;
    weight_t val = 0;
    while (in) {
      getline(in, buf);
      if (buf.size() == 0) continue;
      if (buf[0] == '#') continue;
      if (buf[0] == ' ') {
        cerr << "Weights file lines may not start with whitespace.\n" << buf << endl;
        abort();
      }
      for (int i = buf.size() - 1; i > 0; --i)
        if (buf[i] == '=' || buf[i] == '\t') { buf[i] = ' '; break; }
      int start = 0;
      while(start < buf.size() && buf[start] == ' ') ++start;
      int end = 0;
      while(end < buf.size() && buf[end] != ' ') ++end;
      const int fid = FD::Convert(buf.substr(start, end - start));
      while(end < buf.size() && buf[end] == ' ') ++end;
      val = strtod(&buf.c_str()[end], NULL);
      if (isnan(val)) {
        cerr << FD::Convert(fid) << " has weight NaN!\n";
        abort();
      }
      if (wv_.size() <= fid)
        wv_.resize(fid + 1);
      wv_[fid] = val;
      if (feature_list) { feature_list->push_back(FD::Convert(fid)); }
      ++weight_count;
      if (!SILENT) {
        if (weight_count %   50000 == 0) { cerr << '.' << flush; fl = true; }
        if (weight_count % 2000000 == 0) { cerr << " [" << weight_count << "]\n"; fl = false; }
      }
    }
    if (!SILENT) {
      if (fl) { cerr << endl; }
      cerr << "Loaded " << weight_count << " feature weights\n";
    }
  } else {   // !read_text
    char buf[6];
    in.get(buf, 6);
    size_t num_keys[2];
    in.get(reinterpret_cast<char*>(&num_keys[0]), sizeof(size_t) + 1);
    if (num_keys[0] != FD::NumFeats()) {
      cerr << "Hash function reports " << FD::NumFeats() << " keys but weights file contains " << num_keys[0] << endl;
      abort();
    }
    wv_.resize(num_keys[0]);
    in.get(reinterpret_cast<char*>(&wv_[0]), num_keys[0] * sizeof(weight_t));
    if (!in.good()) {
      cerr << "Error loading weights!\n";
      abort();
    }
  }
}

void Weights::WriteToFile(const std::string& fname, bool hide_zero_value_features, const string* extra) const {
  WriteFile out(fname);
  ostream& o = *out.stream();
  assert(o);
  bool write_text = !FD::UsingPerfectHashFunction();

  if (write_text) {
    if (extra) { o << "# " << *extra << endl; }
    o.precision(17);
    const int num_feats = FD::NumFeats();
    for (int i = 1; i < num_feats; ++i) {
      const weight_t val = (i < wv_.size() ? wv_[i] : 0.0);
      if (hide_zero_value_features && val == 0.0) continue;
      o << FD::Convert(i) << ' ' << val << endl;
    }
  } else {
    o.write("_PHWf", 5);
    const size_t keys = FD::NumFeats();
    assert(keys <= wv_.size());
    o.write(reinterpret_cast<const char*>(&keys), sizeof(keys));
    o.write(reinterpret_cast<const char*>(&wv_[0]), keys * sizeof(weight_t));
  }
}

void Weights::InitVector(std::vector<weight_t>* w) const {
  *w = wv_;
}

void Weights::InitSparseVector(SparseVector<weight_t>* w) const {
  for (int i = 1; i < wv_.size(); ++i) {
    const weight_t& weight = wv_[i];
    if (weight) w->set_value(i, weight);
  }
}

void Weights::InitFromVector(const std::vector<weight_t>& w) {
  wv_ = w;
  if (wv_.size() > FD::NumFeats())
    cerr << "WARNING: initializing weight vector has more features than the global feature dictionary!\n";
  wv_.resize(FD::NumFeats(), 0);
}

void Weights::InitFromVector(const SparseVector<weight_t>& w) {
  wv_.clear();
  wv_.resize(FD::NumFeats(), 0.0);
  for (int i = 1; i < FD::NumFeats(); ++i)
    wv_[i] = w.value(i);
}

