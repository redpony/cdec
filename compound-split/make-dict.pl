#!/usr/bin/perl -w
use strict;
use utf8;
my %d;
my $z = 0;
binmode(STDIN, ":utf8");
binmode(STDOUT, ":utf8");
while(<STDIN>) {
  chomp;
  s/[\–":“„!=+*.@«#%&,»\?\/{}\$\(\)\[\];\-0-9]+/ /g;
  $_ = lc $_;
  my @words = split /\s+/;
  for my $w (@words) {
    next if length($w) == 0;
    $d{$w}++;
    $z++;
  }
}
my $lz = log($z);
for my $w (sort {$d{$b} <=> $d{$a}} keys %d) {
  my $c = $lz-log($d{$w});
  print "$w $c\n";
}

