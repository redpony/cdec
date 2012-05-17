#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef HAVE_CMPH
int main() {
  return 0;
}
#else

#include <iostream>
#include "weights.h"
#include "fdict.h"

using namespace std;

int main(int argc, char** argv) {
  if (argc != 2) { cerr << "Usage: " << argv[0] << " file.mphf\n"; return 1; }
  FD::EnableHash(argv[1]);
  cerr << "Number of keys: " << FD::NumFeats() << endl;
  cerr << "LexFE = " << FD::Convert("LexFE") << endl;
  cerr << "LexEF = " << FD::Convert("LexEF") << endl;
  {
    vector<weight_t> v(FD::NumFeats());
    v[FD::Convert("LexFE")] = 1.0;
    v[FD::Convert("LexEF")] = 0.5;
    cerr << "Writing...\n";
    Weights::WriteToFile("weights.bin", v);
    cerr << "Done.\n";
  }
  {
    vector<weight_t> v(FD::NumFeats());
    cerr << "Reading...\n";
    Weights::InitFromFile("weights.bin", &v);
    cerr << "Done.\n";
    assert(v[FD::Convert("LexFE")] == 1.0);
    assert(v[FD::Convert("LexEF")] == 0.5);
  }
}

#endif

