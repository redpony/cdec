#!/usr/bin/perl -w
use strict;
my %dict=();
while(<>) {
  chomp;
  my ($dummy, $a, $b, $wts) = split / \|\|\| /;
  my @weights = split /\s+/, $wts;
  for my $w (@weights) {
    my ($name, $val) = split /=/, $w;
    unless ($dict{$name}) {
      my $r = (0.5 - rand) / 5;
      $r = sprintf ("%0.4f", $r);
      print "$name $r\n";
      $dict{$name}= 1;
    }
  }
}
