#!/bin/bash

export LANG=C
date
./extractor -i $1 -d X -c 500000 -L 12 -b | sort -t $'\t' -k 1 | gzip > ex.output.gz
date
# -p = compute phrase marginals
# -b = bidirectional rules (starting with F or E) were extracted
gzcat ex.output.gz | ./mr_stripe_rule_reduce -p -b | sort -t $'\t' -k 1 | ./mr_stripe_rule_reduce | gzip > phrase-table.gz
date

