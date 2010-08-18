#!/usr/bin/perl -w
use strict;
my $x = '';
while(<>){
  if (/^$/) { print "\\hline\n"; next; }
  if (/^(\d+)$/) {
    $x=$1;
    $x=~s/^(\d\d\d)(\d\d\d)(\d\d\d)$/$1,$2,$3/;
    $x=~s/^(\d\d)(\d\d\d)(\d\d\d)$/$1,$2,$3/;
    $x=~s/^(\d)(\d\d\d)(\d\d\d)$/$1,$2,$3/;
    $x=~s/^(\d\d\d)(\d\d\d)$/$1,$2/;
    $x=~s/^(\d\d)(\d\d\d)$/$1,$2/;
    $x=~s/^(\d)(\d\d\d)$/$1,$2/;
    next;
  }
  s/ \|\|\| LHSProb.*$//; s/ \|\|\| / \\rightarrow \\langle \\textrm{/; s/\[X(\d+)\]/\\textrm{X}^{$1}/;
  s/ \|\|\| /},\\textrm{{\\emph /;
  chomp;
  print "$x & \$ $_}} \\rangle \$ \\\\\n";
  $x="";
}
