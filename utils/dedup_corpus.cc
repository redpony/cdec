#include <iostream>
#include "hash.h"

using namespace std;

#define INITIAL_SIZE 20000000

int main(int argc, char **argv) {
  if (argc != 1) {
    cerr << "Usage: " << argv[0] << " < file.txt\n";
    return 1;
  }
  SPARSE_HASH_SET<uint64_t> seen(INITIAL_SIZE);
  string line;
  while(getline(cin, line)) {
    uint64_t h = cdec::MurmurHash3_64(&line[0], line.size(), 17);
    if (seen.insert(h).second)
      cout << line << '\n';
  }
}

