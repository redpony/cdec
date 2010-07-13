#!/usr/bin/perl

# dumb grammar coarsener that maps every nonterminal to X (except S).

use strict;

unless (@ARGV > 1){ 
  die "Usage: $0 <weight file> <grammar file> [<grammar file> ... <grammar file>] \n";
}
my $weight_file = shift @ARGV;

$ENV{"LC_ALL"} = "C";
local(*GRAMMAR, *OUT_GRAMMAR, *WEIGHTS);

my %weights;
unless (open(WEIGHTS, $weight_file)) {die "Could not open weight file $weight_file\n" }
while (<WEIGHTS>){
  if (/(.+) (.+)$/){
    $weights{$1} = $2;
  } 
}
close(WEIGHTS);
unless (keys(%weights)){
  die "Could not find any PhraseModel features in weight file (perhaps you specified the wrong file?)\n\n".
    "Usage: $0 <weight file> <grammar file> [<grammar file> ... <grammar file>] \n";
}

sub cleanup_and_die;
$SIG{INT} = "cleanup_and_die";
$SIG{TERM} = "cleanup_and_die"; 
$SIG{HUP} = "cleanup_and_die";

open(OUT_GRAMMAR, ">grammar.tmp");
while (my $grammar_file = shift @ARGV){
  unless (open(GRAMMAR, $grammar_file)) {die "Could not open grammar file $grammar_file\n"}
  while (<GRAMMAR>){
    if (/^((.*\|{3}){3})(.*)$/){
      my $rule = $1;
      my $rest = $3;
      my $coarse_rule = $rule;
      $coarse_rule =~ s/\[X[^\],]*/[X/g;
      print OUT_GRAMMAR "$coarse_rule $rule $rest\n";
    } else {
      die "Unrecognized rule format: $_\n";
    }
  }
  close(GRAMMAR);
}
close(OUT_GRAMMAR);

`sort grammar.tmp > grammar.tmp.sorted`;
sub dump_rules;
sub compute_score;
unless (open(GRAMMAR, "grammar.tmp.sorted")){ die "Something went wrong; could not open intermediate file grammar.tmp.sorted\n"};
my $prev_coarse_rule = "";
my $best_features = "";
my $best_score = 0;
my @rules = ();
while (<GRAMMAR>){
  if (/^\s*((\S.*\|{3}\s*){3})((\S.*\|{3}\s*){3})(.*)$/){
    my $coarse_rule = $1;
    my $fine_rule = $3;
    my $features = $5;  # This code does not correctly handle rules with other info (e.g. alignments)
    if ($coarse_rule eq $prev_coarse_rule){
      my $score = compute_score($features, %weights);
      if ($score > $best_score){
        $best_score = $score;
        $best_features = $features;
      }
    } else {
      dump_rules($prev_coarse_rule, $best_features, @rules);
      $prev_coarse_rule = $coarse_rule;
      $best_features = $features;
      $best_score = compute_score($features, %weights);
      @rules = ();
    }
    push(@rules, "$fine_rule$features\n");
  } else {
    die "Something went wrong during grammar projection: $_\n";
  }
}
dump_rules($prev_coarse_rule, $best_features, @rules);
close(GRAMMAR);
cleanup();

sub compute_score {
  my($features, %weights) = @_;
  my $score = 0;
  if ($features =~ s/^\s*(\S.*\S)\s*$/$1/) { 
    my @features = split(/\s+/, $features);
    my $pm=0;
    for my $feature (@features) {
      my $feature_name; 
      my $feature_val;
      if ($feature =~ /(.*)=(.*)/){
        $feature_name = $1;
        $feature_val= $2;
      } else {
        $feature_name = "PhraseModel_" . $pm;
        $feature_val= $feature;
      }
      $pm++;
      if ($weights{$feature_name}){
        $score += $weights{$feature_name} * $feature_val;
      } 
    }  
  } else {
    die "Unexpected feature value format: $features\n";
  }
  return $score;
}

sub dump_rules {
  my($coarse_rule, $coarse_rule_scores, @fine_rules) = @_;
  unless($coarse_rule){ return; }
  print "$coarse_rule $coarse_rule_scores\n";
  for my $rule (@fine_rules){
    print "\t$rule";
  }
}

sub cleanup_and_die {
  cleanup();
  die "\n";
}

sub cleanup {
 `rm -rf grammar.tmp grammar.tmp.sorted`;
}




