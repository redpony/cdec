pycdec is a Python interface to cdec

## Installation

Build and install pycdec:

	python setup.py install

## Grammar extractor

Compile a parallel corpus and a word alignment into a suffix array representation:

	python -m cdec.sa.compile -f f.txt -e e.txt -a a.txt -o output/ -c extract.ini

Or, if your parallel corpus is in a single-file format (with source and target sentences on a single line, separated by a triple pipe `|||`), use:

	python -m cdec.sa.compile -b f-e.txt -a a.txt -o output/ -c extract.ini

Extract grammar rules from the compiled corpus:
	
	cat input.txt | python -m cdec.sa.extract -c extract.ini -g grammars/

This will create per-sentence grammar files in the `grammars` directory and output annotated input suitable for translation with cdec.
	
## Library usage

A basic demo of pycdec's features is available in `test.py`

More documentation will come as the API becomes stable.

---

pycdec was contributed by [Victor Chahuneau](http://victor.chahuneau.fr)
