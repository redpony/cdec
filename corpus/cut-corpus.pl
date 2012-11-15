#!/usr/bin/perl -w
use strict;
die "Usage: $0 N\nSplits a corpus separated by ||| symbols and returns the Nth field\n" unless scalar @ARGV > 0;

my $x = shift @ARGV;
die "N must be numeric" unless $x =~ /^\d+$/;
$x--;

while(<>) {
  chomp;
  my @fields = split / \|\|\| /;
  my $y = $fields[$x];
  if (!defined $y) { $y= ''; }
  print "$y\n";
}

