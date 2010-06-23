#!/usr/bin/perl -w
use strict;
my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path cwd /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }

use IPC::Run3;
use File::Temp qw ( tempdir );
my $TEMP_DIR = tempdir( CLEANUP => 1 );

my $EXTOOLS = "$SCRIPT_DIR/../../extools";
die "Can't find extools: $EXTOOLS" unless -e $EXTOOLS && -d $EXTOOLS;
my $PYPTOOLS = "$SCRIPT_DIR/../pyp-topics/src";
die "Can't find extools: $PYPTOOLS" unless -e $PYPTOOLS && -d $PYPTOOLS;
my $REDUCER = "$EXTOOLS/mr_stripe_rule_reduce";

my $PYP_TOPICS_TRAIN="$PYPTOOLS/pyp-topics-train";

my $EXTRACTOR = "$EXTOOLS/extractor";
my $FILTER = "$EXTOOLS/filter_grammar";
my $SCORER = "$EXTOOLS/score_grammar";

assert_exec($REDUCER, $EXTRACTOR, $FILTER, $SCORER, $PYP_TOPICS_TRAIN);

usage() unless scalar @ARGV == 1;
open F, "<$ARGV[0]" or die "Can't read $ARGV[0]: $!";
close F;
exit 0;

sub usage {
  print <<EOT;

Usage: $0 [OPTIONS] corpus.fr-en-al

Induces a grammar using Pitman-Yor topic modeling.

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


