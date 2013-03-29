#!/usr/bin/env bash

# Make sure that the sa and online extractors are producing the same (correct) output

set -x verbose

python -m cdec.sa.compile -a corpus.al.gz -b corpus.fr-en.gz -o extract >| extract.ini

cat test.in | python -m cdec.sa.extract -c extract.ini -g gold -o 2>&1 | egrep '\[X\].+\|\|\|.+\|\|\|.+\|\|\|.+\|\|\|'|sed -re 's/INFO.+://g' | ./refmt.py | LC_ALL=C sort >| rules.sort

cd gold && cat grammar.0|sed -re 's/Egiv.+(IsSingletonF=)/\1/g'|LC_ALL=C sort >| rules.sort && cd ..

diff gold/rules.sort gold-rules.sort
diff rules.sort gold-rules.sort
