#!/usr/bin/perl -w
use strict;

die "Usage: $0 x-grammar.scfg[.gz] < cat-grammar.scfg\n" unless scalar @ARGV > 0;

my $xgrammar = shift @ARGV;
die "Can't find $xgrammar" unless -f $xgrammar;
my $fh;
if ($xgrammar =~ /\.gz$/) {
  open $fh, "gunzip -c $xgrammar|" or die "Can't fork: $!";
} else {
  open $fh, "<$xgrammar" or die "Can't read $xgrammar: $!";
}
print STDERR "Reading X-feats from $xgrammar...\n";
my %dict;
while(<$fh>) {
  chomp;
  my ($lhs, $f, $e, $feats) = split / \|\|\| /;
  my $xfeats;
  my $cc = 0;
  my @xfeats = ();
  while ($feats =~ /(EGivenF|FGivenE|LogRuleCount|LogECount|LogFCount|SingletonRule|SingletonE|SingletonF)=([^ ]+)( |$)/og) {
    push @xfeats, "X_$1=$2";
  }
  #print "$lhs ||| $f ||| $e ||| @xfeats\n";
  $dict{"$lhs ||| $f ||| $e"} = "@xfeats";
}
close $fh;

print STDERR "Add features...\n";
while(<>) {
  chomp;
  my ($lhs, $f, $e) = split / \|\|\| /;
  $f=~ s/\[[^]]+,([12])\]/\[X,$1\]/g;
  my $xfeats = $dict{"[X] ||| $f ||| $e"};
  die "Can't find x features for: $_\n" unless $xfeats;
  print "$_ $xfeats\n";
}

