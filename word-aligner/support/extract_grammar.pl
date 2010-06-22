#!/usr/bin/perl -w
use strict;

my $key = shift @ARGV;
die "Usage: $0 KEY\n" unless defined $key;

while(<>) {
  my ($k, @rest) = split / \|\|\| /;
  print join(' ||| ', @rest) if ($k eq $key);
}

