#!/usr/bin/perl -w

use strict;
my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path getcwd /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }
use Getopt::Long;

my $MAX_ITER_ATTEMPTS = 5; # number of times to retry a failed function evaluation
my $CWD=getcwd();
my $OPTIMIZER = "$SCRIPT_DIR/mr_optimize_reduce";
my $DECODER = "$SCRIPT_DIR/../decoder/cdec";
my $COMBINER_CACHE_SIZE = 150;
# This is a hack to run this on a weird cluster,
# eventually, I'll provide Hadoop scripts.
my $PARALLEL = "/chomes/redpony/svn-trunk/sa-utils/parallelize.pl";
die "Can't find $OPTIMIZER" unless -f $OPTIMIZER;
die "Can't execute $OPTIMIZER" unless -x $OPTIMIZER;
my $restart = '';
if ($ARGV[0] && $ARGV[0] eq '--restart') { shift @ARGV; $restart = 1; }

my $pmem="2500mb";
my $nodes = 1;
my $max_iteration = 1000;
my $PRIOR_FLAG = "";
my $parallel = 1;
my $CFLAG = "-C 1";
my $LOCAL;
my $DISTRIBUTED;
my $PRIOR;
my $OALG = "lbfgs";
my $sigsq = 1;
my $means_file;
my $mem_buffers = 20;
my $RESTART_IF_NECESSARY;
GetOptions("cdec=s" => \$DECODER,
           "distributed" => \$DISTRIBUTED,
           "sigma_squared=f" => \$sigsq,
           "lbfgs_memory_buffers=i" => \$mem_buffers,
           "max_iteration=i" => \$max_iteration,
           "means=s" => \$means_file,
           "optimizer=s" => \$OALG,
           "gaussian_prior" => \$PRIOR,
           "restart_if_necessary" => \$RESTART_IF_NECESSARY,
           "jobs=i" => \$nodes,
           "pmem=s" => \$pmem
          ) or usage();
usage() unless scalar @ARGV==3;
my $config_file = shift @ARGV;
my $training_corpus = shift @ARGV;
my $initial_weights = shift @ARGV;
unless ($DISTRIBUTED) { $LOCAL = 1; }
die "Can't find $config_file" unless -f $config_file;
die "Can't find $DECODER" unless -f $DECODER;
die "Can't execute $DECODER" unless -x $DECODER;
if ($LOCAL) { print STDERR "Will run LOCALLY.\n"; $parallel = 0; }
if ($PRIOR) {
  $PRIOR_FLAG="-p --sigma_squared $sigsq";
  if ($means_file) { $PRIOR_FLAG .= " -u $means_file"; }
}

if ($parallel) {
  die "Can't find $PARALLEL" unless -f $PARALLEL;
  die "Can't execute $PARALLEL" unless -x $PARALLEL;
}
unless ($parallel) { $CFLAG = "-C 500"; }
unless ($config_file =~ /^\//) { $config_file = $CWD . '/' . $config_file; }
my $clines = num_lines($training_corpus);
my $dir = "$CWD/ptrain";

if ($RESTART_IF_NECESSARY && -d $dir) {
  $restart = 1;
}

print STDERR <<EOT;
PTRAIN CONFIGURATION INFORMATION

      Config file: $config_file
  Training corpus: $training_corpus
      Corpus size: $clines
  Initial weights: $initial_weights
   Decoder memory: $pmem
   Max iterations: $max_iteration
        Optimizer: $OALG
   Jobs requested: $nodes
           prior?: $PRIOR_FLAG
         restart?: $restart
EOT

if ($OALG) { $OALG="-m $OALG"; }

my $nodelist="1";
for (my $i=1; $i<$nodes; $i++) { $nodelist .= " 1"; }
my $iter = 1;

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
  open T, "<$training_corpus" or die "Can't read $training_corpus: $!";
  open TO, ">$dir/training.in";
  my $lc = 0;
  while(<T>) {
    chomp;
    s/^\s+//;
    s/\s+$//;
    die "Expected A ||| B in input file" unless / \|\|\| /;
    print TO "<seg id=\"$lc\">$_</seg>\n";
    $lc++;
  }
  close T;
  close TO;
}
$training_corpus = "$dir/training.in";

my $iter_attempts = 1;
while ($iter < $max_iteration) {
  my $cur_time = `date`; chomp $cur_time;
  print STDERR "\nStarting iteration $iter...\n";
  print STDERR "  time: $cur_time\n";
  my $start = time;
  my $next_iter = $iter + 1;
  my $dec_cmd="$DECODER -G $CFLAG -c $config_file -w $dir/weights.$iter.gz < $training_corpus 2> $dir/deco.log.$iter";
  my $opt_cmd = "$OPTIMIZER $PRIOR_FLAG -M $mem_buffers $OALG -s $dir/opt.state -i $dir/weights.$iter.gz -o $dir/weights.$next_iter.gz";
  my $pcmd = "$PARALLEL -e $dir/err -p $pmem --nodelist \"$nodelist\" -- ";
  my $cmd = "";
  if ($parallel) { $cmd = $pcmd; }
  $cmd .= "$dec_cmd | $opt_cmd";

  print STDERR "EXECUTING: $cmd\n";
  my $result = `$cmd`;
  my $exit_code = $? >> 8;
  if ($exit_code == 99) {
    $iter_attempts++;
    if ($iter_attempts > $MAX_ITER_ATTEMPTS) {
      die "Received restart request $iter_attempts times from optimizer, giving up\n";
    }
    print STDERR "Function evaluation failed, retrying (attempt $iter_attempts)\n";
    next;
  }
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
  $iter_attempts = 1;
}

print "FINAL WEIGHTS: $dir/weights.$iter\n";
`mv $dir/weights.$iter.gz $dir/weights.final.gz`;

sub usage {
  die <<EOT;

Usage: $0 [OPTIONS] cdec.ini training.corpus weights.init

  Options:

    --distributed      Parallelize function evaluation
    --jobs N           Number of jobs to use
    --cdec PATH        Path to cdec binary
    --optimize OPT     lbfgs, rprop, sgd
    --gaussian_prior   add Gaussian prior
    --means FILE       if you want means other than 0
    --sigma_squared S  variance on prior
    --pmem MEM         Memory required for decoder
    --lbfgs_memory_buffers Number of buffers to use
                           with LBFGS optimizer

EOT
}

sub num_lines {
  my $file = shift;
  my $fh;
  if ($file=~ /\.gz$/) {
    open $fh, "zcat $file|" or die "Couldn't fork zcat $file: $!";
  } else {
    open $fh, "<$file" or die "Couldn't read $file: $!";
  }
  my $lines = 0;
  while(<$fh>) { $lines++; }
  close $fh;
  return $lines;
}
