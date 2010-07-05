#!/usr/bin/perl -w
use strict;

die "Usage: $0 x-grammar.scfg < cat-grammar.scfg\n" unless scalar @ARGV > 0;

my $xgrammar = shift @ARGV;
open F, "<$xgrammar" or die "Can't read $xgrammar: $!";
print STDERR "Reading X-feats from $xgrammar...\n";
my %dict;
while(<F>) {
  chomp;
  my ($lhs, $f, $e, $feats) = split / \|\|\| /;
  my $xfeats;
  my $cc = 0;
  if ($feats =~ /(EGivenF=[^ ]+)( |$)/) {
    $xfeats = "X_$1"; $cc++;
  }
  if ($feats =~ /(FGivenE=[^ ]+)( |$)/) {
    $xfeats = "$xfeats X_$1"; $cc++;
  }
  die "EGivenF and FGivenE features not found: $_" unless $cc == 2;
  #print "$lhs ||| $f ||| $e ||| $xfeats\n";
  $dict{"$lhs ||| $f ||| $e"} = $xfeats;
}
close F;

print STDERR "Add features...\n";
while(<>) {
  chomp;
  my ($lhs, $f, $e) = split / \|\|\| /;
  $f=~ s/\[[^]]+,([12])\]/\[X,$1\]/g;
  my $xfeats = $dict{"[X] ||| $f ||| $e"};
  die "Can't find x features for: $_\n" unless $xfeats;
  print "$_ $xfeats\n";
}

