#!/usr/bin/perl -w

use strict;
my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }
use Getopt::Long;
my $parallel = 0;

my $CWD=`pwd`; chomp $CWD;
my $BIN_DIR = "$CWD/..";
my $REDUCER = "$BIN_DIR/training/mr_em_adapted_reduce";
my $REDUCE2WEIGHTS = "$BIN_DIR/training/mr_reduce_to_weights";
my $ADAPTER = "$BIN_DIR/training/mr_em_map_adapter";
my $DECODER = "$BIN_DIR/decoder/cdec";
my $COMBINER_CACHE_SIZE = 10000000;
my $PARALLEL = "/chomes/redpony/svn-trunk/sa-utils/parallelize.pl";
die "Can't find $REDUCER" unless -f $REDUCER;
die "Can't execute $REDUCER" unless -x $REDUCER;
die "Can't find $REDUCE2WEIGHTS" unless -f $REDUCE2WEIGHTS;
die "Can't execute $REDUCE2WEIGHTS" unless -x $REDUCE2WEIGHTS;
die "Can't find $ADAPTER" unless -f $ADAPTER;
die "Can't execute $ADAPTER" unless -x $ADAPTER;
die "Can't find $DECODER" unless -f $DECODER;
die "Can't execute $DECODER" unless -x $DECODER;
my $restart = '';
if ($ARGV[0] && $ARGV[0] eq '--restart') { shift @ARGV; $restart = 1; }

die "Usage: $0 [--restart] training.corpus cdec.ini\n" unless (scalar @ARGV == 2);

my $training_corpus = shift @ARGV;
my $config = shift @ARGV;
my $pmem="2500mb";
my $nodes = 40;
my $max_iteration = 1000;
my $CFLAG = "-C 1";
if ($parallel) {
  die "Can't find $PARALLEL" unless -f $PARALLEL;
  die "Can't execute $PARALLEL" unless -x $PARALLEL;
} else { $CFLAG = "-C 500"; }

my $initial_weights = '';

print STDERR <<EOT;
EM TRAIN CONFIGURATION INFORMATION

      Config file: $config
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

  if ($initial_weights) {
    unless ($initial_weights =~ /\.gz$/) {
      `cp $initial_weights $dir/weights.1`;
      `gzip -9 $dir/weights.1`;
    } else {
      `cp $initial_weights $dir/weights.1.gz`;
    }
  }
}

while ($iter < $max_iteration) {
  my $cur_time = `date`; chomp $cur_time;
  print STDERR "\nStarting iteration $iter...\n";
  print STDERR "  time: $cur_time\n";
  my $start = time;
  my $next_iter = $iter + 1;
  my $WSTR = "-w $dir/weights.$iter.gz";
  if ($iter == 1) { $WSTR = ''; }
  my $dec_cmd="$DECODER --feature_expectations -c $config $WSTR $CFLAG < $training_corpus 2> $dir/deco.log.$iter";
  my $pcmd = "$PARALLEL -e $dir/err -p $pmem --nodelist \"$nodelist\" -- ";
  my $cmd = "";
  if ($parallel) { $cmd = $pcmd; }
  $cmd .= "$dec_cmd";
  $cmd .= "| $ADAPTER | sort -k1 | $REDUCER | $REDUCE2WEIGHTS -o $dir/weights.$next_iter.gz";
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

