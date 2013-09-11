#!/usr/bin/env bash

ROOTDIR=`dirname $0`
SUPPORT=$ROOTDIR/support

if [[ $# == 1 && $1 == '-u' ]] ; then
    NORMARGS="--batchline"
else
    NORMARGS=""
fi

$SUPPORT/utf8-normalize.sh $NORMARGS |
  $SUPPORT/quote-norm.pl |
  $SUPPORT/tokenizer.pl |
  sed -u -e 's/ al - / al-/g' |
  $SUPPORT/fix-contract.pl |
  sed -u -e 's/^ //' | sed -u -e 's/ $//' |
  perl -e '$|++; while(<>){s/(\d+)(\.+)$/$1 ./; s/(\d+)(\.+) \|\|\|/$1 . |||/;  print;}'

