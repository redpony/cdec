#!/bin/bash

# this is the location on malbec, if you want to run on another machine
# ICU may be installed in /usr or /usr/local
ICU_DIR=/usr0/tools/icu
UCONV_BIN=$ICU_DIR/bin/uconv
UCONV_LIB=$ICU_DIR/lib

if [ -e $UCONV_BIN ] && [ -d $UCONV_LIB ]
then
  export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$UCONV_LIB
  if [ ! -x $UCONV_BIN ]
  then
    echo "$0: Cannot execute $UCONV_BIN! Please fix." 1>&2
    exit
  fi
  CMD="$UCONV_BIN -f utf8 -t utf8 -x Any-NFKC --callback skip"
else
  if which uconv > /dev/null
  then
    CMD="uconv -f utf8 -t utf8 -x Any-NFKC --callback skip"
  else
    echo "$0: Cannot find ICU uconv (http://site.icu-project.org/) ... falling back to iconv. Quality may suffer." 1>&2
    CMD="iconv -f utf8 -t utf8 -c"
  fi
fi

if [[ $# == 1 && $1 == "--batchline" ]]; then
    perl $(dirname $0)/utf8-normalize-batch.pl "$CMD"
else
    perl -e '$|++; while(<>){s/\r\n*/\n/g; print;}' \
    |$CMD \
    |/usr/bin/perl -w -e '
        $|++;
        while (<>) {
            chomp;
            s/[\x00-\x1F]+/ /g;
            s/  +/ /g;
            s/^ //;
            s/ $//;
            print "$_\n";
        }'
fi
