#!/usr/bin/perl -w
use strict;

my @xx;
my @yy;
my @os;
my $sec = undef;
my $i = 0;
while(<>) {
  chomp;
  if (/^\s*$/) {
    print "<seg id=\"$i\"";
    $i++;
    for (my $j = 0; $j < $sec; $j++) {
      my @oo = ();
      for (my $k = 0; $k < scalar @xx; $k++) {
        my $sym = $os[$k]->[$j];
        $sym =~ s/"/'/g;
        push @oo, $sym;
      }
      my $zz = $j + 1;
      print " feat$zz=\"@oo\"";
    }

    print "> @xx ||| @yy </seg>\n";
    @xx = ();
    @yy = ();
    @os = ();
  } else {
    my ($x, @fs) = split /\s+/;
    my $y = pop @fs;
    if (!defined $sec) { $sec = scalar @fs; }
    die unless $sec == scalar @fs;
    push @xx, $x;
    push @yy, $y;
    push @os, \@fs;
  }
}

