#!/usr/bin/perl -w
use strict;
die "Usage: $0 corpus.e|f corpus.f|e" unless scalar @ARGV == 2;

my ($a, $b) = @ARGV;
open A, "<$a" or die "Can't read $a: $!";
open B, "<$b" or die "Can't read $a: $!";

while(<A>) {
  chomp;
  my $e = <B>;
  die "Mismatched lines in $a and $b!" unless defined $e;
  print "$_ ||| $e";
}

my $e = <B>;
die "Mismatched lines in $a and $b!" unless !defined $e;

