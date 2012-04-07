#include "arc_factored.h"

#include <iostream>

using namespace std;

int main(int argc, char** argv) {
  ArcFactoredForest af(5);
  cerr << af(0,3) << endl;
  return 0;
}

