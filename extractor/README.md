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
