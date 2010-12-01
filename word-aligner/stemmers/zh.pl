#!/usr/bin/perl -w

use strict;
use utf8;

binmode(STDIN, ":utf8");
binmode(STDOUT,":utf8");

my $vocab = undef;
if (scalar @ARGV > 0) {
  die "Only allow --vocab" unless ($ARGV[0] eq '--vocab' && scalar @ARGV == 1);
  $vocab = 1;
}

my %dict;
while(<STDIN>) {
  chomp;
  my @words = split /\s+/;
  my @out = @words;
  if ($vocab) {
    die "Expected exactly one word per line with --vocab: $_" unless scalar @out == 1;
    print "$_ @out\n";
  } else {
    print "@out\n";
  }
}

