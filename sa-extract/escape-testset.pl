#!/usr/bin/perl -w

use utf8;
use strict;

binmode(STDIN,":utf8");
binmode(STDOUT,":utf8");

my @fh = ();
if (scalar @ARGV == 0) {
  push @fh, \*STDIN;
} else {
  for my $file (@ARGV) {
    my $f;
    open $f, "<$file" or die "Can't read $file: $!\n";
    binmode $f, ":utf8";
    push @fh, $f;
  }
}

my $id = -1;
for my $f (@fh) {
  while(<$f>) {
    chomp;
    die "Empty line in test set" if /^\s*$/;
    die "Please remove <seg> tags from input:\n$_" if /^\s*<seg/i;
    $id++;
    s/&/\&amp;/g;
    s/</\&lt;/g;
    s/>/\&gt;/g;
    print "<seg id=\"$id\"> $_ </seg>\n";
  }
}


