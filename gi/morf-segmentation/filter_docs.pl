#!/usr/bin/perl

#Filters the phrase&cluster document set to retain only documents that correspond to words or morphs, i.e. not crossing word boundaries.

#Usage: filter_docs.pl [mark]
#  STDIN: data in the doc.txt format (i.e. phrase\t blahblah ), most likely from cdec extractor
#  STDOUT: the matching subset, same format

use utf8;
my $letter=qr/\p{L}\p{M}*/; # see http://www.regular-expressions.info/unicode.html

my $morph=qr/$letter+/;

my $m = "##"; # marker used to indicate morphemes
if ((scalar @ARGV) >= 1) {
   $m = $ARGV[0];
   shift;
}
print STDERR "Using $m to filter for morphemes\n";

my $expr = qr/^($morph\Q$m\E)? ?(\Q$m\E$morph\Q$m\E)* ?(\Q$m\E$morph)?\t/; #\Q and \E bounded sections are escaped
while(<>) {
   /$expr/ && print;
}
