#!/usr/bin/perl -w

# [X] ||| so [X,1] die [X,2] der ||| as [X,1] existing [X,2] the ||| 2.47712135315 2.53182387352 5.07100057602 ||| 0-0 2-2 4-4
# [X] ||| so [X,1] die [X,2] der ||| this [X,1] the [X,2] of ||| 2.47712135315 3.19828724861 2.38270020485 ||| 0-0 2-2 4-4
# [X] ||| so [X,1] die [X,2] der ||| as [X,1] the [X,2] the ||| 2.47712135315 2.53182387352 1.48463630676 ||| 0-0 2-2 4-4
# [X] ||| so [X,1] die [X,2] der ||| is [X,1] the [X,2] of the ||| 2.47712135315 3.45197868347 2.64251494408 ||| 0-0 2-2 4-4 4-5

die "Usage: 0 model1.f-e model1.e-f < grammar.scfg\n" unless scalar @ARGV == 2;

my $fm1 = shift @ARGV;
die unless $fm1;
my $frm1 = shift @ARGV;
die unless $frm1;
open M1,"<$fm1" or die;
open RM1,"<$frm1" or die;
print STDERR "Loading Model 1 probs from $fm1...\n";
my %m1;
while(<M1>) {
  chomp;
  my ($f, $e, $lp) = split /\s+/;
  $m1{$e}->{$f} = exp($lp);
}
close M1;

print STDERR "Loading Inverse Model 1 probs from $frm1...\n";
my %rm1;
while(<RM1>) {
  chomp;
  my ($e, $f, $lp) = split /\s+/;
  $rm1{$f}->{$e} = exp($lp);
}
close RM1;

my @label = qw( EGivenF LexFGivenE LexEGivenF );
while(<>) {
  chomp;
  my ($l, $f, $e, $sscores, $al) = split / \|\|\| /;
  my @scores = split /\s+/, $sscores;
  for (my $i=0; $i<3; $i++) { $scores[$i] = "$label[$i]=$scores[$i]"; }
  my @fs = split /\s+/, $f;
  my @es = split /\s+/, $e;
  my $flen = scalar @fs;
  my $elen = scalar @es;
  my $pgen = 0;
  my $nongen = 0;
  for (my $i =0; $i < $flen; $i++) {
    my $ftot = 0;
    next if ($fs[$i] =~ /\[X/);
    my $cr = $rm1{$fs[$i]};
    for (my $j=0; $j <= $elen; $j++) {
      my $ej = '<eps>';
      if ($j < $elen) { $ej = $es[$j]; }
      my $p = $cr->{$ej};
      if (defined $p) { $ftot += $p; }
    }
    if ($ftot == 0) { $nongen = 1; last; }
    $pgen += log($ftot) - log($elen);
  }
  unless ($nongen) { push @scores, "RGood=1"; } else { push @scores, "RBad=1"; }

  $nongen = 0;
  $pgen = 0;
  for (my $i =0; $i < $elen; $i++) {
    my $etot = 0;
    next if ($es[$i] =~ /\[X/);
    my $cr = $m1{$es[$i]};
#    print STDERR "$es[$i]\n";
    for (my $j=0; $j <= $flen; $j++) {
      my $fj = '<eps>';
      if ($j < $flen) { $fj = $fs[$j]; }
      my $p = $cr->{$fj};
#      print STDERR "  $fs[$j] : $p\n";
      if (defined $p) { $etot += $p; }
    }
    if ($etot == 0) { $nongen = 1; last; }
    $pgen += log($etot) - log($flen);
  }
  unless ($nongen) { push @scores, "FGood=1"; } else { push @scores, "FBad=1"; }
  print "$l ||| $f ||| $e ||| @scores ||| $al\n";
}

