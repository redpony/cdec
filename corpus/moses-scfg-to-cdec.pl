#!/usr/bin/perl -w
use strict;

while(<>) {
  my ($src, $trg, $feats, $al) = split / \|\|\| /;
  # [X][NP] von [X][NP] [X] ||| [X][NP] &apos;s [X][NP] [S] ||| 0.00110169 0.0073223 2.84566e-06 0.0027702 0.0121867 2.718 0.606531 ||| 0-0 1-1 2-2 ||| 635 245838 2

  my @srcs = split /\s+/, $src;
  my @trgs = split /\s+/, $trg;
  my $lhs = pop @trgs;
  $lhs =~ s/&amp;apos;/'/g;
  $lhs =~ s/&apos;/'/g;
  $lhs =~ s/,/COMMA/g;
  my $ntc = 0;
  my $sc = 0;
  my @of = ();
  my $x = pop @srcs;
  my %d = ();  # src index to nonterminal count
  die "Expected [X]" unless $x eq '[X]';
  my %amap = ();
  my @als = split / /, $al;
  for my $st (@als) {
    my ($s, $t) = split /-/, $st;
    $amap{$t} = $s;
  }
  for my $f (@srcs) {
    if ($f =~ /^\[X\]\[([^]]+)\]$/) {
      $ntc++;
      my $nt = $1;
      $nt =~ s/&amp;apos;/'/g;
      $nt =~ s/&apos;/'/g;
      $nt =~ s/,/COMMA/g;
      push @of, "[$nt]";
      $d{$sc} = $ntc;
    } elsif ($f =~ /^\[[^]]+\]$/) {
      die "Unexpected $f";
    } else {
      push @of, $f;
    }
    $sc++;
  }
  my @oe = ();
  my $ind = 0;
  for my $e (@trgs) {
    if ($e =~ /^\[X\]\[([^]]+)\]$/) {
      my $imap = $d{$amap{$ind}};
      push @oe, "[$imap]";
    } else {
      push @oe, $e;
    }
    $ind++;
  }
  my ($fe, $ef, $j, $lfe, $lef, $dummy, $of) = split / /, $feats;
  next if $lef eq '0';
  next if $lfe eq '0';
  next if $ef eq '0';
  next if $fe eq '0';
  next if $j eq '0';
  next if $of eq '0';
  $ef = sprintf('%.6g', log($ef));
  $fe = sprintf('%.6g',log($fe));
  $j = sprintf('%.6g',log($j));
  $lef = sprintf('%.6g',log($lef));
  $lfe = sprintf('%.6g',log($lfe));
  $of = sprintf('%.6g',log($of));
  print "$lhs ||| @of ||| @oe ||| RuleCount=1 FgivenE=$fe EgivenF=$ef Joint=$j LexEgivenF=$lef LexFgivenE=$lfe Other=$of\n";
}

# [X][ADVP] angestiegen [X] ||| rose [X][ADVP] [VP] ||| 0.0538131 0.0097508 0.00744224 0.0249653 0.000698602 2.718 0.606531 ||| 0-1 1-0 ||| 13 94 2
