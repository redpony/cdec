#!/bin/bash

MYDIR=$(dirname $0)

export LANG=C
date 1>&2
$MYDIR/extractor -i $1 -c 500000 -L 12 -C | sort -t $'\t' -k 1 | $MYDIR/mr_stripe_rule_reduce
date 1>&2

