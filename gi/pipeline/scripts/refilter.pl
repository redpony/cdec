#!/usr/bin/perl -w
use strict;

my $NUM_TRANSLATIONS = shift @ARGV;
unless ($NUM_TRANSLATIONS) { $NUM_TRANSLATIONS=30; }
print STDERR "KEEPING $NUM_TRANSLATIONS TRANSLATIONS FOR SOURCE\n";

my $pk = '';
my %dict;
while(<>) {
  s/^(.+)\t//;
  my $key = $1;
  if ($key ne $pk) {
    if ($pk) {
      emit_dict();
    }
    %dict = ();
    $pk = $key;
  }
  my ($lhs, $f, $e, $s) = split / \|\|\| /;
  my $score = 0;
  if ($s =~ /XEF=([^ ]+)/) {
    $score += $1;
  } else { die; }
  if ($s =~ /GenerativeProb=([^ ]+)/) {
    $score += ($1 / 10);
  } else { die; }
  $dict{"$lhs ||| $f ||| $e ||| $s"} = $score;
}
emit_dict();

sub emit_dict {
  my $cc = 0;
  for my $k (sort { $dict{$a} <=> $dict{$b} } keys %dict) {
    print "$k";
    $cc++;
    if ($cc >= $NUM_TRANSLATIONS) { last; }
  }
}

