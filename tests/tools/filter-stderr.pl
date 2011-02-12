#!/usr/bin/perl -w
use strict;

my $REAL = '(\+|-)?[1-9][0-9]*(\.[0-9]*)?|(\+|-)?[1-9]\.[0-9]*e(\+|-)?[0-9]+';

while(<>) {
if (/Init.*forest\s+\(nodes\/edges\): (\d+)\/(\d+)/) { print "-lm_nodes $1\n-lm_edges $2\n"; }
if (/Init.*forest\s+\(paths\): (.+)$/) { print "-lm_paths $1\n"; }
# -LM Viterbi: -12.7893
if (/Init.*\s+Viterbi:\s+($REAL)/) {
  print "-lm_viterbi $1\n";
} elsif (/-LM\s+Viterbi:\s+(.+)$/) {
  # -LM Viterbi: australia is have diplomatic relations with north korea one of the few countries .
  print "-lm_trans $1\n";
}
#Constr. forest (nodes/edges): 111/305
#Constr. forest       (paths): 9899
if (/Constr\. forest\s+\(nodes\/edges\): (\d+)\/(\d+)/) { print "constr_nodes $1\nconstr_edges $2\n"; }
if (/Constr\. forest\s+\(paths\): (.+)$/) { print "constr_paths $1\n"; }

if (/Pass1.*forest\s+\(nodes\/edges\): (\d+)\/(\d+)/) { print "+lm_nodes $1\n+lm_edges $2\n"; }
if (/Pass1.*forest\s+\(paths\): (.+)$/) { print "+lm_paths $1\n"; }
if (/Pass1.*Viterbi:\s+($REAL)/) {
  print "+lm_viterbi $1\n";
} elsif (/Pass1.*Viterbi:\s+(.+)$/) {
  # -LM Viterbi: australia is have diplomatic relations with north korea one of the few countries .
  print "+lm_trans $1\n";
}

}
