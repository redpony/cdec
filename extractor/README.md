C++ implementation of the online grammar extractor originally developed by [Adam Lopez](http://www.cs.jhu.edu/~alopez/).

The grammar extraction takes place in two steps: (a) precomputing a number of data structures and (b) actually extracting the grammars. All the flags below have the same meaning as in the cython implementation.

To compile the data structures you need to run:

    cdec/extractor/compile -a <alignment> -b <parallel_corpus> -c <compile_config_file> -o <compile_directory>

To extract the grammars you need to run:

    cdec/extract/extract -t <num_threads> -c <compile_config_file> -g <grammar_output_path> < <input_sentencs> > <sgm_file>

To run unit tests you need first to configure `cdec` with the [Google Test](https://code.google.com/p/googletest/) and [Google Mock](https://code.google.com/p/googlemock/) libraries:

    ./configure --with-gtest=</absolute/path/to/gtest> --with-gmock=</absolute/path/to/gmock>

Then, you simply need to:

    cd cdec/extractor
    make check
