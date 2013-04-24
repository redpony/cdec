C++ implementation of the online grammar extractor originally developed by [Adam Lopez](http://www.cs.jhu.edu/~alopez/).

To run the extractor you need to:

    cdec/extractor/run_extractor -a <alignment> -b <parallel_corpus> -g <grammar_output_path> < <input_sentences> > <sgm_file>

To run unit tests you need first to configure `cdec` with the [Google Test](https://code.google.com/p/googletest/) and [Google Mock](https://code.google.com/p/googlemock/) libraries:

    ./configure --with-gtest=</absolute/path/to/gtest> --with-gmock=</absolute/path/to/gmock>

Then, you simply need to:

    cd cdec/extractor
    make check
