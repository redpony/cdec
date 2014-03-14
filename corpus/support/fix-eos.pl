#!/usr/bin/perl -w
$|++;

use strict;
use utf8;

binmode(STDIN, ":utf8");
binmode(STDOUT, ":utf8");
while(<STDIN>) {
  s/(\p{Devanagari}{2}[A-Za-z0-9! ,.\@\p{Devanagari}]+?)\s+(\.)(\s*$|\s+\|\|\|)/$1 \x{0964}$3/s;
  print;
}
