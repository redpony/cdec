#!/usr/bin/perl -w

use strict;
use utf8;
use Getopt::Long;

my $model_file;
my $src_file;
my $hyp_file;
my $help;
my $reverse_model;
my $feature_name='M1SrcGivenTrg';

Getopt::Long::Configure("no_auto_abbrev");
if (GetOptions(
    "model_file|m=s" => \$model_file,
    "source_file|s=s" => \$src_file,
    "feature_name|f=s" => \$feature_name,
    "hypothesis_file|h=s" => \$hyp_file,
    "help" => \$help,
) == 0 || @ARGV!=0 || $help || !$model_file || !$src_file || !$hyp_file) {
  usage();
  exit;
}

binmode STDIN, ":utf8";
binmode STDOUT, ":utf8";
binmode STDERR, ":utf8";

print STDERR "Reading Model 1 probabilities from $model_file...\n";
open M, "<$model_file" or die "Couldn't read $model_file: $!";
binmode M, ":utf8";
my %m1;
while(<M>){
  chomp;
  my ($e,$f,$lp) = split /\s+/;
  die unless defined $e;
  die unless defined $f;
  die unless defined $lp;
  $m1{$f}->{$e} = $lp;
}
close M;

open SRC, "<$src_file" or die "Can't read $src_file: $!";
open HYP, "<$hyp_file" or die "Can't read $hyp_file: $!";
binmode(SRC,":utf8");
binmode(HYP,":utf8");
binmode(STDOUT,":utf8");
my @source; while(<SRC>){chomp; push @source, $_; }
close SRC;
my $src_len = scalar @source;
print STDERR "Read $src_len sentences...\n";
print STDERR "Rescoring...\n";

my $cur = undef;
my @hyps = ();
my @feats = ();
while(<HYP>) {
  chomp;
  my ($id, $hyp, $feats) = split / \|\|\| /;
  unless (defined $cur) { $cur = $id; }
  die "sentence ids in k-best list file must be between 0 and $src_len" if $id < 0 || $id > $src_len;
  if ($cur ne $id) {
    rescore($cur, $source[$cur], \@hyps, \@feats);
    $cur = $id;
    @hyps = ();
    @feats = ();
  }
  push @hyps, $hyp;
  push @feats, $feats;
}
rescore($cur, $source[$cur], \@hyps, \@feats) if defined $cur;

sub rescore {
  my ($id, $src, $rh, $rf) = @_;
  my @hyps = @$rh;
  my @feats = @$rf;
  my $nhyps = scalar @hyps;
  my %cache = ();
  print STDERR "RESCORING SENTENCE id=$id (# hypotheses=$nhyps)...\n";
  for (my $i=0; $i < $nhyps; $i++) {
    my $score = $cache{$hyps[$i]};
    if (!defined $score) {
      if ($reverse_model) {
        die "not implemented";
      } else {
        $score = m1_prob($src, $hyps[$i]);
      }
      $cache{$hyps[$i]} = $score;
    }
    print "$id ||| $hyps[$i] ||| $feats[$i] $feature_name=$score\n";
  }

}

sub m1_prob {
  my ($fsent, $esent) = @_;
  die unless defined $fsent;
  die unless defined $esent;
  my @fwords = split /\s+/, $fsent;
  my @ewords = split /\s+/, $esent;
  push @ewords, "<eps>";
  my $tp = 0;
  for my $f (@fwords) {
    my $m1f = $m1{$f};
    if (!defined $m1f) { $m1f = {}; }
    my $tfp = 0;
    for my $e (@ewords) {
      my $lp = $m1f->{$e};
      if (!defined $lp) { $lp = -100; }
      #print "P($f|$e) = $lp\n";
      my $prob = exp($lp);
      #if ($prob > $tfp) { $tfp = $prob; }
      $tfp += $prob;
    }
    $tp += log($tfp);
    $tp -= log(scalar @ewords);  # uniform probability of each generating word
  }
  return $tp;
}

sub usage {
  print STDERR "Usage: $0 -m model_file.txt -h hypothesis.nbest -s source.txt\n  Adds the back-translation probability under Model 1\n  Use training/model1 to generate the required parameter file\n";
}


