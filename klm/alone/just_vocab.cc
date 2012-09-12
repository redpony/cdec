#include "alone/read.hh"
#include "util/file_piece.hh"

#include <iostream>

int main() {
  util::FilePiece f(0, "stdin", &std::cerr);
  while (true) {
    try {
      alone::JustVocab(f, std::cout);
    } catch (const util::EndOfFileException &e) { break; }
    std::cout << '\n';
  }
}
