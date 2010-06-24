#!/usr/bin/perl -w
use strict;
my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path cwd /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }

my $GZIP = 'gzip';
my $ZCAT = 'gunzip -c';

my $EXTOOLS = "$SCRIPT_DIR/../../extools";
die "Can't find extools: $EXTOOLS" unless -e $EXTOOLS && -d $EXTOOLS;

my $FILTER = "$EXTOOLS/filter_grammar";
my $SCORE = "$EXTOOLS/score_grammar";

assert_exec($FILTER, $SCORE);

usage() unless scalar @ARGV == 3;
my $corpus = $ARGV[0];
my $grammar = $ARGV[1];
my $testset = $ARGV[2];
die "Can't find corpus: $corpus" unless -f $corpus;
die "Can't find corpus: $grammar" unless -f $grammar;
die "Can't find corpus: $testset" unless -f $testset;
print STDERR "  CORPUS: $corpus\n";
print STDERR " GRAMMAR: $corpus\n";
print STDERR "TEST SET: $corpus\n";
print STDERR "Extracting...\n";

safesystem("$ZCAT $grammar | $FILTER $testset | $SCORE $corpus") or die "Failed";

sub usage {
  print <<EOT;

Usage: $0 corpus.src_trg_al grammar.gz test-set.txt > filtered-grammar.scfg.txt

Filter and score a grammar for a test set.

EOT
  exit 1;
};

sub assert_exec {
  my @files = @_;
  for my $file (@files) {
    die "Can't find $file - did you run make?\n" unless -e $file;
    die "Can't execute $file" unless -e $file;
  }
};

sub safesystem {
  print STDERR "Executing: @_\n";
  system(@_);
  if ($? == -1) {
      print STDERR "ERROR: Failed to execute: @_\n  $!\n";
      exit(1);
  }
  elsif ($? & 127) {
      printf STDERR "ERROR: Execution of: @_\n  died with signal %d, %s coredump\n",
          ($? & 127),  ($? & 128) ? 'with' : 'without';
      exit(1);
  }
  else {
    my $exitcode = $? >> 8;
    print STDERR "Exit code: $exitcode\n" if $exitcode;
    return ! $exitcode;
  }
}


