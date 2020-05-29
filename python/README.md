pycdec is a Python interface to cdec 

## Installation

Build and install pycdec:

	python setup.py install

Alternatively, run `python setup.py build_ext --inplace` and add the `python/` directory to your `PYTHONPATH`.

To re-build pycdec from the cython source, modify setup.py in the following ways:
  * Add this input statement: from Cython.Build import cythonize
  * Change the source file from cdec/\_cdec.cpp to cdec/\_cdec.pyx
  * Add language='c++' as a property to ext\_modules (e.g. right after extra\_link\_args)
  * In the final setup block, change ext\_modules=ext\_modules to ext\_modules=cythonize(ext\_modules)

Then just build and install normally, as described above.

To rebuild cdec/\_cdec.cpp, run:

 	cython --cplus \_cdec.pyx

## Grammar extractor

Compile a parallel corpus and a word alignment into a suffix array representation:

	python -m cdec.sa.compile -f f.txt -e e.txt -a a.txt -o output/ -c extract.ini

Or, if your parallel corpus is in a single-file format (with source and target sentences on a single line, separated by a triple pipe `|||`), use:

	python -m cdec.sa.compile -b f-e.txt -a a.txt -o output/ -c extract.ini

Extract grammar rules from the compiled corpus:
	
	cat input.txt | python -m cdec.sa.extract -c extract.ini -g grammars/ -z

This will create per-sentence grammar files in the `grammars` directory and output annotated input suitable for translation with cdec.

Extract rules in stream mode:

    python -m cdec.sa.extract -c extract.ini -t -z	

This will enable stdio interaction with the following types of lines:

Extract grammar:

    context ||| sentence ||| grammar_file

Learn (online mode, specify context name):

    context ||| sentence ||| reference ||| alignment

Drop (online mode, specify context name):

    context ||| drop

## Library usage

A basic demo of pycdec's features is available in `examples/test.py`.
Other examples are given in [the paper](http://victor.chahuneau.fr/pub/pycdec/) describing pycdec.

More documentation will come as the API becomes stable.

---

pycdec was contributed by [Victor Chahuneau](http://victor.chahuneau.fr)
