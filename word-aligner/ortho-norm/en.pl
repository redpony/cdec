#!/usr/bin/perl -w
use strict;
use utf8;

while(<STDIN>) {
  $_ = lc $_;
  s/ al-/ al/g;
  s/^al-/al/;
  print;
}

