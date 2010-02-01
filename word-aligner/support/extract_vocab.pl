#!/usr/bin/perl -w
use strict;

print STDERR "Extracting vocabulary...\n";
my %dict = ();
my $wc = 0;
while(<>) {
  chomp;
  my @words = split /\s+/;
  for my $word (@words) { $wc++; $dict{$word}++; }
}

my $tc = 0;
for my $word (sort {$dict{$b} <=> $dict{$a}} keys %dict) {
  print "$word\n";
  $tc++;
}

print STDERR "$tc types / $wc tokens\n";

