#!/usr/bin/perl -w
$|++;

use strict;
while(<>) {
  #s/ (pre|anti|re|pro|inter|intra|multi|e|x|neo) - / $1- /ig;
  #s/ - (year) - (old)/ -$1-$2/ig;
  s/ ' (s|m|ll|re|d|ve) / '$1 /ig;
  s/n ' t / n't /ig;
  s/( |^)(\d+)-(\d+)( |$)/$1$2 - $3$4/g;
  s/ "$/ ”/;
  print;
}

