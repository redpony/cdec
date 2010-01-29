#!/usr/bin/perl -w
use strict;
use utf8;

binmode(STDIN, ":utf8");
binmode(STDOUT, ":utf8");

while(<STDIN>) {
  $_ = lc $_;
  # see http://en.wikipedia.org/wiki/Use_of_the_circumflex_in_French
  s/â/as/g;
  s/ê/es/g;
  s/î/is/g;
  s/ô/os/g;
  s/û/us/g;

  s/ç/c/g;
  s/é|è/e/g;
  s/á/a/g;
  print;
}

