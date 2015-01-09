#!/bin/bash

# This script uses ICU uconv (http://site.icu-project.org/), if it's available
# to normalize UTF8 text into a standard form. For information about this
# process, refer to http://en.wikipedia.org/wiki/Unicode_equivalence#Normalization
# Escape characters between 0x00-0x1F are removed

if which uconv > /dev/null
then
  CMD="uconv -f utf8 -t utf8 -x Any-NFKC --callback skip --remove-signature"
else
  echo "Cannot find ICU uconv (http://site.icu-project.org/) ... falling back to iconv. Normalization NOT taking place." 1>&2
  CMD="iconv -f utf8 -t utf8 -c"
fi

$CMD | /usr/bin/perl -w -e '
 while (<>) {
     chomp;
      s/[\x00-\x1F]+/ /g;
      s/  +/ /g;
      s/^ //;
      s/ $//;
      print "$_\n";
    }'

