#!/usr/bin/perl -w
use strict;

while(<>) {
  chomp;
  my @fields = split / \|\|\| /;
  my ($ff, $ee, $aa) = @fields;
  die "Expected: foreign ||| target ||| alignments" unless scalar @fields == 3;
  my @fs = split /\s+/, $ff;
  my @es = split /\s+/, $ee;
  my @as = split /\s+/, $aa;
  my @oas = ();
  push @oas, '0-0';
  my $flen = scalar @fs;
  my $elen = scalar @es;
  for my $ap (@as) {
    my ($a, $b) = split /-/, $ap;
    die "Bad format in: @as" unless defined $a && defined $b;
    push @oas, ($a + 1) . '-' . ($b + 1);
  }
  push @oas, ($flen + 1) . '-' . ($elen + 1);
  print "<s> $ff </s> ||| <s> $ee </s> ||| @oas\n";
}

