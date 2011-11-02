#!/usr/bin/perl -w

use strict;
use utf8;

binmode(STDIN, ":utf8");
binmode(STDOUT,":utf8");

my $vocab = undef;
if (scalar @ARGV > 0) {
  die "Only allow --vocab" unless ($ARGV[0] eq '--vocab' && scalar @ARGV == 1);
  $vocab = 1;
}

my %dict;
while(<STDIN>) {
  chomp;
  my @words = split /\s+/;
  my @out = ();
  for my $w (@words) {
    my $tw = $dict{$w};
    if (!defined $tw) {
      my $el = 5;
      if ($w =~ /(ndz|ndr|nts|ntr)/) { $el++; }
      if ($w =~ /^(mp|mb|nd)/) { $el++; }
      if ($el > length($w)) { $el = length($w); }
      $tw = substr $w, 0, $el;
      $dict{$w} = $tw;
    }
    push @out, $tw;
  }
  if ($vocab) {
    die "Expected exactly one word per line with --vocab: $_" unless scalar @out == 1;
    print "$_ @out\n";
  } else {
    print "@out\n";
  }
}

