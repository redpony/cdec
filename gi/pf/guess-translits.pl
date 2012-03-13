#!/usr/bin/perl -w
use strict;
use utf8;

my $MIN_PMI = -3;

my %fs;
my %es;
my %ef;

die "Usage: $0 < input.utf8.txt\n" if scalar @ARGV > 0;

binmode(STDIN,":utf8");
binmode(STDOUT,":utf8");
binmode(STDERR,":utf8");

my $tot = 0;
print STDERR "Reading alignments from STDIN ...\n";
while(<STDIN>) {
  chomp;
  my ($fsent, $esent, $alsent) = split / \|\|\| /;
  die "Format should be 'foreign sentence ||| english sentence ||| 0-0 1-1 ...'\n" unless defined $fsent && defined $esent && defined $alsent;

  my @fws = split /\s+/, $fsent;  
  my @ews = split /\s+/, $esent;
  my @as = split /\s+/, $alsent;
  my %a2b;
  my %b2a;
  for my $ap (@as) {
    my ($a,$b) = split /-/, $ap;
    die "BAD INPUT: $_\n" unless defined $a && defined $b;
    $a2b{$a}->{$b} = 1;
    $b2a{$b}->{$a} = 1;
  }
  for my $a (keys %a2b) {
    my $bref = $a2b{$a};
    next unless scalar keys %$bref < 2;
    my $b = (keys %$bref)[0];
    next unless scalar keys %{$b2a{$b}} < 2;
    my $f = $fws[$a];
    next unless defined $f;
    next unless length($f) > 3;
    my $e = $ews[$b];
    next unless defined $e;
    next unless length($e) > 3;

    $ef{$f}->{$e}++;
    $es{$e}++;
    $fs{$f}++;
    $tot++;
  }  
}
my $ltot = log($tot);
my $num = 0;
print STDERR "Extracting pairs for PMI > $MIN_PMI ...\n";
for my $f (keys %fs) {
  my $logf = log($fs{$f});
  my $esref = $ef{$f};
  for my $e (keys %$esref) {
    my $loge = log($es{$e});
    my $ef = $esref->{$e};
    my $logef = log($ef);
    my $pmi = $logef - ($loge + $logf);
    next if $pmi < $MIN_PMI;
    my @flets = split //, $f;
    my @elets = split //, $e;
    print "@flets ||| @elets\n";
    $num++;
  }
}
print STDERR "Extracted $num pairs.\n";
print STDERR "Recommend running:\n   ../../training/model1 -v -d -t -99999 output.txt\n";
