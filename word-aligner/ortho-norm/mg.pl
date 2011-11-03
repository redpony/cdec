#!/usr/bin/perl -w
use strict;
use utf8;

binmode(STDIN, ":utf8");
binmode(STDOUT, ":utf8");

while(<STDIN>) {
  $_ = lc $_;
  s/([a-z])'( |$)/$1$2/g;
  print;
}

