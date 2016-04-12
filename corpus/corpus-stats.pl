#!/usr/bin/perl -w
use strict;

my $f = <>;
my $IS_PARALLEL = ($f =~ / \|\|\| /);
if ($IS_PARALLEL) {
  die "This script is only valid for monolingual corpora, but file contains |||\n";
}

my %d;
my $tc = 0;
my $lc = 0;
while($f) {
  $lc++;
  chomp $f;
  my @toks = split /\s+/, $f;
  for my $t (@toks) {
    $d{$t}++;
    $tc++;
  }
  $f=<>;
}

my $types = scalar keys %d;
my $ttr = $tc / $types;
my @mfts;
for my $k (sort {$d{$b} <=> $d{$a}} keys %d) {
  push @mfts, $k;
  last if scalar @mfts > 24;
}
my $sing = 0;
for my $k (keys %d) {
  if ($d{$k} == 1) { $sing++; }
}
my $stypes = sqrt($types);

print <<EOT;
CORPUS STATISTICS

          Lines: $lc
         Tokens: $tc
          Types: $types
    sqrt(types): $stypes
 Type-tok ratio: $ttr
     Singletons: $sing

Most freq types: @mfts

EOT

