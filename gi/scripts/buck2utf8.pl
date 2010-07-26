#!/usr/bin/perl -w
use strict;
use utf8;
binmode(STDOUT, ":utf8");
while(<>) {
  chomp;
  my @words = split /\s+/;
  for my $w (@words) {
    $_ = $w;
    if ($w =~ /^__NTK__/o) {
      s/__NTK__//go;
      next if /^$/;
      print STDOUT "$_ ";
      next;
    }
s/tR/\x{0679}/g;  # retroflex t
s/dR/\x{0688}/g;  # retroflex d
s/rR/\x{0691}/g;  # retroflex r
s/p/\x{067E}/g;   # peh
s/c/\x{0686}/g;   # tcheh
s/g/\x{06AF}/g;   # geh (G=ghain)
s/@/\x{06BE}/g;   # heh doachashmee
s/h'/\x{06c2}/g;  # heh goal + hamza
s/h/\x{06c1}/g;   # heh goal
s/J/\x{0698}/g;   # zheh (rare, usually persian loan words)
s/k/\x{06A9}/g;   # k
s/Y'/\x{06d3}/g;  # yeh barree + hamza above (ligature)
s/y/\x{06cc}/g;   # same as ya' in arabic
s/Y/\x{06d2}/g;   # yeh barree
s/N/\x{06BA}/g;  # Ghunna

    s/\'/\x{0621}/g;
    s/\|/\x{0622}/g;
    s/\>/\x{0623}/g;
    s/\&/\x{0624}/g;
    s/\</\x{0625}/g;
    s/\}/\x{0626}/g;
    s/A/\x{0627}/g;
    s/b/\x{0628}/g;
    s/t/\x{062A}/g;
    s/v/\x{062B}/g;
    s/j/\x{062C}/g;
    s/H/\x{062D}/g;
    s/x/\x{062E}/g;
    s/d/\x{062F}/g;
    s/\*/\x{0630}/g;
    s/r/\x{0631}/g;
    s/z/\x{0632}/g;
    s/s/\x{0633}/g;
    s/\$/\x{0634}/g;
    s/S/\x{0635}/g;
    s/D/\x{0636}/g;
    s/T/\x{0637}/g;
    s/Z/\x{0638}/g;
    s/E/\x{0639}/g;
    s/g/\x{063A}/g;
    s/_/\x{0640}/g;
    s/f/\x{0641}/g;
    s/q/\x{0642}/g;
    s/k/\x{0643}/g;
    s/l/\x{0644}/g;
    s/m/\x{0645}/g;
    s/n/\x{0646}/g;
    s/h/\x{0647}/g;
    s/w/\x{0648}/g;
    s/Y/\x{0649}/g;
    s/y/\x{064A}/g;
    s/F/\x{064B}/g;
    s/N/\x{064C}/g;
    s/K/\x{064D}/g;
    s/a/\x{064E}/g;
    s/u/\x{064F}/g;
    s/i/\x{0650}/g;
    s/\~/\x{0651}/g;
    s/o/\x{0652}/g;
    s/\`/\x{0670}/g;
    s/\{/\x{0671}/g;
    s/P/\x{067E}/g;
    s/J/\x{0686}/g;
    s/V/\x{06A4}/g;
    s/G/\x{06AF}/g;


print STDOUT "$_ ";
  }
  print STDOUT "\n";
}
