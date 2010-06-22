#!/usr/bin/perl -w
use strict;
use utf8;

binmode(STDIN, ":utf8");
binmode(STDOUT, ":utf8");

while(<STDIN>) {
  chomp;
  my @out = ();
  my @words = split /\s+/;
  for my $of (@words) {
    if (length($of) > 1 && !($of =~ /\d/)) {
      $of =~ s/\$/sh/g;
    }
    $of =~ s/([a-z])\~/$1$1/g;
    $of =~ s/E/'/g;
    $of =~ s/^Aw/o/;
    $of =~ s/\|/a/g;
    $of =~ s/@/h/g;
    $of =~ s/c/ch/g;
    $of =~ s/x/kh/g;
    $of =~ s/\*/dh/g;
    $of =~ s/p$/a/;
    $of =~ s/w/o/g;
    $of =~ s/Z/dh/g;
    $of =~ s/y/i/g;
    $of =~ s/Y/a/g;
    $of = lc $of;
    push @out, $of;
  }
  print "@out\n";
}

