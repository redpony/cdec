#!/usr/bin/perl -w
use strict;
use utf8;

die "Usage: $0 f.voc corpus.f-e grammar.f-e.gz\n" unless scalar @ARGV == 3;

my $MAX_INMEM = 3000;

open FV,"<$ARGV[0]" or die "Can't read $ARGV[0]: $!";
open C,"<$ARGV[1]" or die "Can't read $ARGV[1]: $!";
open G,"gunzip -c $ARGV[2]|" or die "Can't read $ARGV[2]: $!";

binmode FV, ":utf8";
binmode C, ":utf8";
binmode G, ":utf8";

my $vc = 0;
my %most_freq;
$most_freq{"<eps>"} = 1;
while(my $f = <FV>) {
  chomp $f;
  $most_freq{$f}=1;
  $vc++;
  last if $vc == $MAX_INMEM;
}
close FV;

print STDERR "Loaded $vc vocabulary items for permanent translation cache\n";

my %grammar;
my $memrc = 0;
my $loadrc = 0;
while(<G>) {
  chomp;
  my ($f, $e, $feats) = split / \|\|\| /;
  if ($most_freq{$f}) {
    #print "$_\n";
    $memrc++;
  } else {
    $loadrc++;
    my $r = $grammar{$f};
    if (!defined $r) {
      $r = [];
      $grammar{$f} = $r;
    }
    push @$r, "$e ||| $feats";
  }
}
close G;
print STDERR "  mem rc: $memrc\n";
print STDERR " load rc: $loadrc\n";

while(<C>) {
  my ($f,$e) = split / \|\|\| /;
  my @fwords = split /\s+/, $f;
  my $tot = 0;
  for my $f (@fwords) {
    my $r = $grammar{$f};
    die "No translations for: $f" unless $r;
    my $num = scalar @$r;
    $tot += $num;
  }
  print "RULES: $tot\n";
}
