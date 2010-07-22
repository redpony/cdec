#!/usr/bin/perl

while(<>) {
  my ($lhs, $f, $e, $s) = split / \|\|\| /;
  $f =~ s/\[X[0-9]+\]/\[X\]/g;
  print "$f\t$_";
}

