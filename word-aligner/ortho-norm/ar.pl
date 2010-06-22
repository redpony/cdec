#!/usr/bin/perl -w
use strict;
use utf8;

binmode(STDIN, ":utf8");
binmode(STDOUT, ":utf8");

while(<STDIN>) {
  chomp;
  my $len = length($_);
  if ($len > 1 && !($_ =~ /\d/)) {
    s/\$/sh/g;
  }
  s/([a-z])\~/$1$1/g;
  s/E/'/g;
  s/^Aw/o/g;
  s/\|/a/g;
  s/@/h/g;
  s/c/ch/g;
  s/x/kh/g;
  s/\*/dh/g;
  s/w/o/g;
  s/v/th/g;
  if ($len > 1) { s/}/'/g; }
  s/Z/dh/g;
  s/y/i/g;
  s/Y/a/g;
  if ($len > 1) { s/p$//; }
  $_ = lc $_;
  print "$_\n";
}

