#include <iostream>

#include "filelib.h"
#include "decoder.h"
#include "ff_register.h"

using namespace std;

int main(int argc, char** argv) {
  register_feature_functions();
  Decoder decoder(argc, argv);

  const string input = decoder.GetConf()["input"].as<string>();
  cerr << "Reading input from " << ((input == "-") ? "STDIN" : input.c_str()) << endl;
  ReadFile in_read(input);
  istream *in = in_read.stream();
  assert(*in);

  string buf;
  while(*in) {
    getline(*in, buf);
    if (buf.empty()) continue;
    decoder.Decode(buf);
  }
  return 0;
}

