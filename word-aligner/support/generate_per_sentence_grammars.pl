#!/usr/bin/perl -w
use strict;
use utf8;

die "Usage: $0 f.voc corpus.f-e grammar.f-e.gz [OUT]filtered.f-e.gz [OUT]per_sentence_grammar.f-e [OUT]train.f-e.sgml\n" unless scalar @ARGV == 6;

my $MAX_INMEM = 2500;

open FV,"<$ARGV[0]" or die "Can't read $ARGV[0]: $!";
open C,"<$ARGV[1]" or die "Can't read $ARGV[1]: $!";
open G,"gunzip -c $ARGV[2]|" or die "Can't read $ARGV[2]: $!";

open FILT,"|gzip -c > $ARGV[3]" or die "Can't write $ARGV[3]: $!";
open PSG,">$ARGV[4]" or die "Can't write $ARGV[4]: $!";
open OTRAIN,">$ARGV[5]" or die "Can't write $ARGV[5]: $!";

binmode OTRAIN, ":utf8";
binmode FILT, ":utf8";
binmode PSG, ":utf8";
binmode STDOUT, ":utf8";
binmode STDERR, ":utf8";
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
    print FILT "$_\n";
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
close FILT;
close G;
print STDERR "  mem rc: $memrc\n";
print STDERR " load rc: $loadrc\n";

my $id = 0;
while(<C>) {
  chomp;
  my ($f,$e) = split / \|\|\| /;
  my @fwords = split /\s+/, $f;
  my $tot = 0;
  my %used;
  my $fpos = tell(PSG);
  for my $f (@fwords) {
    next if $most_freq{$f};
    next if $used{$f};
    my $r = $grammar{$f};
    die "No translations for: $f" unless $r;
    my $num = scalar @$r;
    $tot += $num;
    for my $rule (@$r) {
      print PSG "$f ||| $rule\n";
    }
    $used{$f} = 1;
  }
  print PSG "###EOS###\n";
  print OTRAIN "<seg id=\"$id\" psg=\"\@$fpos\"> $_ </seg>\n";
  $id++;
}
close PSG;
close OTRAIN;
print STDERR "Done.\n";

