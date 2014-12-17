#!/usr/bin/env bash

ROOTDIR=`dirname $0`
SUPPORT=$ROOTDIR/support

if [[ $# == 1 && $1 == '-u' ]] ; then
    NORMARGS="--batchline"
    SEDFLAGS="-u"
else
    if [[ $# != 0 ]] ; then
        echo Usage: `basename $0` [-u] \< file.in \> file.out 1>&2
        echo 1>&2
        echo Tokenizes text in a reasonable way in most languages. 1>&2
        echo 1>&2
        exit 1
    fi
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

