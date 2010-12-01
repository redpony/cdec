#!/usr/bin/perl -w
use utf8;
use strict;

my $LIMIT_SIZE=30;

my ($effile, $model1, $imodel1) = @ARGV;
die "Usage: $0 corpus.fr-en corpus.f-e.model1 corpus.e-f.model1" unless $effile && -f $effile && $model1 && -f $model1 && $imodel1 && -f $imodel1;

my $ADD_NULL = 1;

open EF, "<$effile" or die;
open M1, "<$model1" or die;
open IM1, "<$imodel1" or die;
binmode(EF,":utf8");
binmode(M1,":utf8");
binmode(IM1,":utf8");
binmode(STDOUT,":utf8");
my %model1;
print STDERR "Reading model1...\n";
my %sizes = ();
while(<M1>) {
  chomp;
  my ($f, $e, $lp) = split /\s+/;
  $model1{$f}->{$e} = 1;
  $sizes{$f}++;
}
close M1;

my $inv_add = 0;
my %invm1;
print STDERR "Reading inverse model1...\n";
my %esizes=();
while(<IM1>) {
  chomp;
  my ($e, $f, $lp) = split /\s+/;
  $invm1{$e}->{$f} = 1;
  $esizes{$e}++;
  if (($sizes{$f} or 0) < $LIMIT_SIZE && !(defined $model1{$f}->{$e})) {
    $model1{$f}->{$e} = 1;
    $sizes{$f}++;
    $inv_add++;
  }
}
close IM1;
print STDERR "Added $inv_add from inverse model1\n";

print STDERR "Generating grammars...\n";

my %fdict;
while(<EF>) {
  chomp;
  my ($f, $e) = split /\s*\|\|\|\s*/;
  my @es = split /\s+/, $e;
  my @fs = split /\s+/, $f;
  for my $ew (@es){
    die "E: Empty word" if $ew eq '';
  }
  push @fs, '<eps>' if $ADD_NULL;
  my $i = 0;
  for my $fw (@fs){
    $i++;
    die "F: Empty word\nI=$i FS: @fs" if $fw eq '';
  }
  for my $fw (@fs){
    for my $ew (@es){
      $fdict{$fw}->{$ew}++;
    }
  }
}

my %model4;
#while(<M4>) {
#  my $en = <M4>; chomp $en;
#  my $zh = <M4>; chomp $zh;
#  die unless $zh =~ /^NULL \({/;
#  my @ewords = split /\s+/, $en;
#  my @chunks = split /\}\) ?/, $zh;
#
#  for my $c (@chunks) {
#    my ($zh, $taps) = split / \(\{ /, $c;
#    if ($zh eq 'NULL') { $zh = '<eps>'; }
#    my @aps = map { $ewords[$_ - 1]; } (split / /, $taps);
#    #print "$zh -> @aps\n";
#    for my $ap (@aps) {
#      $model4{$zh}->{$ap} += 1;
#    }
#  }
#}
#close M4;

my $specials = 0;
my $fc = 1000000;
my $sids = 1000000;
for my $f (sort keys %fdict) {
  my $re = $fdict{$f};
  my $max;
  for my $e (sort {$re->{$b} <=> $re->{$a}} keys %$re) {
    my $efcount = $re->{$e};
    unless (defined $max) { $max = $efcount; }
    my $m1 = $model1{$f}->{$e};
    my $m4 = $model4{$f}->{$e};
    my $im1 = $invm1{$e}->{$f};
    my $is_good_pair = (defined $m1 || defined $m4);
    my $ident = ($e eq $f);
    if ($ident) { $is_good_pair = 1; }
    next unless $is_good_pair;
    print "$f ||| $e ||| X=0\n" if $is_good_pair;
  }
}

