#!/usr/bin/perl -w
use strict;

# ADDS SELF-TRANSLATIONS OF POORLY ATTESTED WORDS TO THE PARALLEL DATA

my %df;
my %def;
while(<>) {
#  print;
  chomp;
  my ($sf, $se) = split / \|\|\| /;
  die "Format error: $_\n" unless defined $sf && defined $se;
  my @fs = split /\s+/, $sf;
  my @es = split /\s+/, $se;
  for my $f (@fs) {
    $df{$f}++;
    for my $e (@es) {
      if ($f eq $e) { $def{$f}++; }
    }
  }
}

for my $k (sort keys %def) {
  next if $df{$k} > 4;
  print "$k ||| $k\n";
  print "$k ||| $k\n";
  print "$k ||| $k\n";
}

