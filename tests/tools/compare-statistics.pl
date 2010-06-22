#!/usr/bin/perl -w
use strict;

die "Usage: $0 gold.statistics < test.statistics\n" unless scalar @ARGV == 1;

my $gold_file = shift @ARGV;
open G, "<$gold_file" or die "Can't read $gold_file: $!";
my @gold_keys = ();
my @gold_vals = ();
while(<G>) {
  chomp;
  if (/^([^ ]+)\s*(.*)$/) {
    push @gold_keys, $1;
    push @gold_vals, $2;
  } else {
    die "Unexpected line in $gold_file: $_\n";
  }
}

my $sc = 0;
my $MATCH = 0;
my $MISMATCH = 0;
while(<>) {
  my $gold_key = $gold_keys[$sc];
  my $gold_val = $gold_vals[$sc];
  $sc++;
  if (/^([^ ]+)\s*(.*)$/) {
    my $test_key = $1;
    my $test_val = $2;
    if ($test_key ne $gold_key) {
      die "Missing key in output! Expected '$gold_key' but got '$test_key'\n";
    }
    if ($gold_val ne 'IGNORE') {
      if ($gold_val eq $test_val) { $MATCH++; } else {
        $MISMATCH++;
        print STDERR "[VALUE FAILURE] key: '$gold_key'\n    expected value: '$gold_val'\n      actual value: '$test_val'\n";
      }
    }
  } else {
    die "Unexpected line in test data: $_\n";
  }
}

my $TOT = $MISMATCH + $MATCH;

print "$MATCH $TOT\n";

if ($MISMATCH > 0) { exit 1; } else { exit 0; }

