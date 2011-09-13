#include "config.h"

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
    Weights w;
    vector<weight_t> v(FD::NumFeats());
    v[FD::Convert("LexFE")] = 1.0;
    v[FD::Convert("LexEF")] = 0.5;
    w.InitFromVector(v);
    cerr << "Writing...\n";
    w.WriteToFile("weights.bin");
    cerr << "Done.\n";
  }
  {
    Weights w;
    vector<weight_t> v(FD::NumFeats());
    cerr << "Reading...\n";
    w.InitFromFile("weights.bin");
    cerr << "Done.\n";
    w.InitVector(&v);
    assert(v[FD::Convert("LexFE")] == 1.0);
    assert(v[FD::Convert("LexEF")] == 0.5);
  }
}

#endif

