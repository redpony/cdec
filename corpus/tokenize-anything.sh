#!/bin/sh

ROOTDIR=`dirname $0`
SUPPORT=$ROOTDIR/support

$SUPPORT/utf8-normalize.sh |
  $SUPPORT/quote-norm.pl |
  $SUPPORT/tokenizer.pl |
  sed -e 's/ al - / al-/g' |
  $SUPPORT/fix-contract.pl |
  sed -e 's/^ //' | sed -e 's/ $//' |
  perl -e 'while(<>){s/(\d+)(\.+)$/$1 ./;print;}'

