#!/usr/bin/env bash

ROOTDIR=`dirname $0`
SUPPORT=$ROOTDIR/support

if [[ $# == 1 && $1 == '-u' ]] ; then
    NORMARGS="--batchline"
    SEDFLAGS="-u"
else
    NORMARGS=""
    SEDFLAGS=""
fi

$SUPPORT/utf8-normalize.sh $NORMARGS |
  $SUPPORT/quote-norm.pl |
  $SUPPORT/tokenizer.pl |
  $SUPPORT/fix-eos.pl |
  sed $SEDFLAGS -e 's/ al - / al-/g' |
  $SUPPORT/fix-contract.pl |
  sed $SEDFLAGS -e 's/^ //' | sed $SEDFLAGS -e 's/ $//' |
  perl -e '$|++; while(<>){s/(\d+)(\.+)$/$1 ./; s/(\d+)(\.+) \|\|\|/$1 . |||/;  print;}'

