#!/usr/bin/perl -w

use strict;
my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }
use Getopt::Long;
my $parallel = 1;

my $CWD=`pwd`; chomp $CWD;
my $BIN_DIR = "/chomes/redpony/cdyer-svn-repo/cdec/src";
my $OPTIMIZER = "$BIN_DIR/mr_em_train";
my $DECODER = "$BIN_DIR/cdec";
my $COMBINER_CACHE_SIZE = 150;
my $PARALLEL = "/chomes/redpony/svn-trunk/sa-utils/parallelize.pl";
die "Can't find $OPTIMIZER" unless -f $OPTIMIZER;
die "Can't execute $OPTIMIZER" unless -x $OPTIMIZER;
die "Can't find $DECODER" unless -f $DECODER;
die "Can't execute $DECODER" unless -x $DECODER;
die "Can't find $PARALLEL" unless -f $PARALLEL;
die "Can't execute $PARALLEL" unless -x $PARALLEL;
my $restart = '';
if ($ARGV[0] && $ARGV[0] eq '--restart') { shift @ARGV; $restart = 1; }

die "Usage: $0 [--restart] training.corpus weights.init grammar.file [grammar2.file] ...\n" unless (scalar @ARGV >= 3);

my $training_corpus = shift @ARGV;
my $initial_weights = shift @ARGV;
my @in_grammar_files = @ARGV;
my $pmem="2500mb";
my $nodes = 40;
my $max_iteration = 1000;
my $CFLAG = "-C 1";
unless ($parallel) { $CFLAG = "-C 500"; }
my @grammar_files;
for my $g (@in_grammar_files) {
  unless ($g =~ /^\//) { $g = $CWD . '/' . $g; }
  die "Can't find $g" unless -f $g;
  push @grammar_files, $g;
}

print STDERR <<EOT;
EM TRAIN CONFIGURATION INFORMATION

  Grammar file(s): @grammar_files
  Training corpus: $training_corpus
  Initial weights: $initial_weights
   Decoder memory: $pmem
  Nodes requested: $nodes
   Max iterations: $max_iteration
          restart: $restart
EOT

my $nodelist="1";
for (my $i=1; $i<$nodes; $i++) { $nodelist .= " 1"; }
my $iter = 1;

my $dir = "$CWD/emtrain";
if ($restart) {
  die "$dir doesn't exist, but --restart specified!\n" unless -d $dir;
  my $o = `ls -t $dir/weights.*`;
  my ($a, @x) = split /\n/, $o;
  if ($a =~ /weights.(\d+)\.gz$/) {
    $iter = $1;
  } else {
    die "Unexpected file: $a!\n";
  }
  print STDERR "Restarting at iteration $iter\n";
} else {
  die "$dir already exists!\n" if -e $dir;
  mkdir $dir or die "Can't create $dir: $!";

  unless ($initial_weights =~ /\.gz$/) {
    `cp $initial_weights $dir/weights.1`;
    `gzip -9 $dir/weights.1`;
  } else {
    `cp $initial_weights $dir/weights.1.gz`;
  }
}

while ($iter < $max_iteration) {
  my $cur_time = `date`; chomp $cur_time;
  print STDERR "\nStarting iteration $iter...\n";
  print STDERR "  time: $cur_time\n";
  my $start = time;
  my $next_iter = $iter + 1;
  my $gfile = '-g' . (join ' -g ', @grammar_files);
  my $dec_cmd="$DECODER --feature_expectations -S 999 $CFLAG $gfile -n -w $dir/weights.$iter.gz < $training_corpus 2> $dir/deco.log.$iter";
  my $opt_cmd = "$OPTIMIZER $gfile -o $dir/weights.$next_iter.gz";
  my $pcmd = "$PARALLEL -e $dir/err -p $pmem --nodelist \"$nodelist\" -- ";
  my $cmd = "";
  if ($parallel) { $cmd = $pcmd; }
  $cmd .= "$dec_cmd | $opt_cmd";

  print STDERR "EXECUTING: $cmd\n";
  my $result = `$cmd`;
  if ($? != 0) {
    die "Error running iteration $iter: $!";
  }
  chomp $result;
  my $end = time;
  my $diff = ($end - $start);
  print STDERR "  ITERATION $iter TOOK $diff SECONDS\n";
  $iter = $next_iter;
  if ($result =~ /1$/) {
    print STDERR "Training converged.\n";
    last;
  }
}

print "FINAL WEIGHTS: $dir/weights.$iter\n";

