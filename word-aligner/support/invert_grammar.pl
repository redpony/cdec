#!/usr/bin/perl -w
use strict;

while(<>) {
  my ($f, $e, $scores) = split / \|\|\| /;
  print "$e ||| $f ||| $scores";
}

