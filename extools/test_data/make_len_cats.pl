#!/usr/bin/perl -w
use strict;

my $max_len = 15;
my @cat_names = qw( NULL SHORT SHORT MID MID MID LONG LONG LONG LONG LONG VLONG VLONG VLONG VLONG VLONG );

while(<>) {
  chomp;
  my @words = split /\s+/;
  my $len = scalar @words;
  my @spans;
  for (my $i =0; $i < $len; $i++) {
    for (my $k = 1; $k <= $max_len; $k++) {
      my $j = $i + $k;
      next if ($j > $len);
      my $cat = $cat_names[$k];
      die unless $cat;
      push @spans, "$i-$j:$cat";
    }
  }
  print "@spans\n";
}

