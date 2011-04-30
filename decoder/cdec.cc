#include <iostream>

#include "filelib.h"
#include "decoder.h"
#include "ff_register.h"
#include "verbose.h"

using namespace std;

int main(int argc, char** argv) {
  register_feature_functions();
  Decoder decoder(argc, argv);

  const string input = decoder.GetConf()["input"].as<string>();
  const bool show_feature_dictionary = decoder.GetConf().count("show_feature_dictionary");
  if (!SILENT) cerr << "Reading input from " << ((input == "-") ? "STDIN" : input.c_str()) << endl;
  ReadFile in_read(input);
  istream *in = in_read.stream();
  assert(*in);

  string buf;
  while(*in) {
    getline(*in, buf);
    if (buf.empty()) continue;
    decoder.Decode(buf);
  }
  if (show_feature_dictionary) {
    int num = FD::NumFeats();
    for (int i = 1; i < num; ++i) {
      cout << FD::Convert(i) << endl;
    }
  }
  return 0;
}

