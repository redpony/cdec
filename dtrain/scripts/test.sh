#!/bin/sh

EXP=$1
#head -5000
cat ./data/in.blunsom08 | ./dtest -q false -c ./data/cdec.ini -w ./data/weights-$EXP 2> ./output/err.$EXP > ./output/out.$EXP

