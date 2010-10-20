#!/usr/bin/perl -w
use strict;
use utf8;

while(<STDIN>) {
  $_ = lc $_;
  s/ al-/ al/g;
  s/^al-/al/;
  s/j/i/g;
  s/dž/j/g;
  s/š/sh/g;
  s/á/a/g;
  s/č/ch/g;
  s/í/i/g;
  print;
}

