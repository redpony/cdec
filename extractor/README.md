A simple and fast C++ implementation of a SCFG grammar extractor using suffix arrays. The implementation is described in this [paper](https://ufal.mff.cuni.cz/pbml/102/art-baltescu-blunsom.pdf). The original cython extractor is described in [Adam Lopez](http://www.cs.jhu.edu/~alopez/)'s PhD [thesis](http://www.cs.jhu.edu/~alopez/papers/adam.lopez.dissertation.pdf).

The grammar extraction takes place in two steps: (a) precomputing a number of data structures and (b) actually extracting the grammars. All the flags below have the same meaning as in the cython implementation.

To compile the data structures you need to run:

    cdec/extractor/sacompile -a <alignment> -b <parallel_corpus> -c <compile_config_file> -o <compile_directory>

To extract the grammars you need to run:

    cdec/extract/extract -t <num_threads> -c <compile_config_file> -g <grammar_output_path> < <input_sentencs> > <sgm_file>

To run unit tests you need first to configure `cdec` with the [Google Test](https://code.google.com/p/googletest/) and [Google Mock](https://code.google.com/p/googlemock/) libraries:

    ./configure --with-gtest=</absolute/path/to/gtest> --with-gmock=</absolute/path/to/gmock>

Then, you simply need to:

    cd cdec/extractor
    make check

To run the extractor as a daemon for online grammar extraction you need to compile with [nanomsg](http://nanomsg.org/download.html) in your system's PATH and then:

    cdec/extractor/extract_daemon -c <compile_config_file> -g <grammar_output_path> -a <address>

Online here means that sentences are received in an online fashion and rather than loading the files from <compile_config_file> anew when a new sentence is received, they are stored in RAM and a new grammar can be requested via the daemon. The daemon can be killed using the SID supplied in the log file.

To then query the daemon you need to implement the Requester class supplied in
extractor/libextract_request.a. It's constructor takes the same address string as supplied for the daemon. E.g.:

    #include <iostream>

    #include "extract_request.h"

    using namespace std;

    int main(int argc, char** argv) {
      extractor::Requester requester("ipc:///tmp/extract_daemon.ipc");
      cout << requester.request_for_sentence("<input sentence>") << endl;
      return 0;
    }

which can be compiled with:

    g++ extract_request_test.cc -o run_test -lextract_request -L/workspace/osm/cdec/extractor/ -I/workspace/osm/cdec/extractor/ -lnanomsg
