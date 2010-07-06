#!/usr/bin/env perl

use strict;
my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }
use Getopt::Long;
use IPC::Open2;
use strict;
use POSIX ":sys_wait_h";
#my $QSUB_FLAGS = "-q batch -l pmem=3000mb,walltime=5:00:00";
my $QSUB_FLAGS = "-l mem_free=9G";

# Default settings
my $srcFile;
my $refFiles;
my $bin_dir = $SCRIPT_DIR;
die "Bin directory $bin_dir missing/inaccessible" unless -d $bin_dir;
my $FAST_SCORE="$bin_dir/fast_score";
die "Can't execute $FAST_SCORE" unless -x $FAST_SCORE;
my $MAPINPUT = "$bin_dir/mr_vest_generate_mapper_input";
my $MAPPER = "$bin_dir/mr_vest_map";
my $REDUCER = "$bin_dir/mr_vest_reduce";
my $parallelize = "$bin_dir/parallelize.pl";
my $SCORER = $FAST_SCORE;
die "Can't find $MAPPER" unless -x $MAPPER;
my $cdec = "$bin_dir/../decoder/cdec";
die "Can't find decoder in $cdec" unless -x $cdec;
die "Can't find $parallelize" unless -x $parallelize;
my $decoder = $cdec;
my $lines_per_mapper = 400;
my $rand_directions = 15;
my $iteration = 1;
my $run_local = 0;
my $best_weights;
my $max_iterations = 15;
my $optimization_iters = 6;
my $decode_nodes = 15;   # number of decode nodes
my $pmem = "9g";
my $disable_clean = 0;
my %seen_weights;
my $normalize;
my $help = 0;
my $epsilon = 0.0001;
my $interval = 5;
my $dryrun = 0;
my $last_score = -10000000;
my $metric = "ibm_bleu";
my $dir;
my $iniFile;
my $weights;
my $initialWeights;
my $decoderOpt;

# Process command-line options
Getopt::Long::Configure("no_auto_abbrev");
if (GetOptions(
	"decoder=s" => \$decoderOpt,
	"decode-nodes=i" => \$decode_nodes,
	"dont-clean" => \$disable_clean,
	"dry-run" => \$dryrun,
	"epsilon" => \$epsilon,
	"help" => \$help,
	"interval" => \$interval,
	"iteration=i" => \$iteration,
	"local" => \$run_local,
	"max-iterations=i" => \$max_iterations,
	"normalize=s" => \$normalize,
	"pmem=s" => \$pmem,
	"rand-directions=i" => \$rand_directions,
	"ref-files=s" => \$refFiles,
	"metric=s" => \$metric,
	"source-file=s" => \$srcFile,
	"weights=s" => \$initialWeights,
	"workdir=s" => \$dir
) == 0 || @ARGV!=1 || $help) {
	print_help();
	exit;
}

if ($metric =~ /^(combi|ter)$/i) {
  $lines_per_mapper = 40;
}

($iniFile) = @ARGV;


sub write_config;
sub enseg;
sub print_help;

my $nodelist;
my $host =`hostname`; chomp $host;
my $bleu;
my $interval_count = 0;
my $logfile;
my $projected_score;

# used in sorting scores
my $DIR_FLAG = '-r';
if ($metric =~ /^ter$|^aer$/i) {
  $DIR_FLAG = '';
}

my $refs_comma_sep = get_comma_sep_refs($refFiles);

unless ($dir){ 
	$dir = "vest";
}
unless ($dir =~ /^\//){  # convert relative path to absolute path
	my $basedir = `pwd`;
	chomp $basedir;
	$dir = "$basedir/$dir";
}

if ($decoderOpt){ $decoder = $decoderOpt; }


# Initializations and helper functions
srand;

my @childpids = ();
my @cleanupcmds = ();

sub cleanup {
	print STDERR "Cleanup...\n";
	for my $pid (@childpids){ `kill $pid`; }
	for my $cmd (@cleanupcmds){`$cmd`; }
	exit 1;
};
$SIG{INT} = "cleanup";
$SIG{TERM} = "cleanup"; 
$SIG{HUP} = "cleanup";

my $decoderBase = `basename $decoder`; chomp $decoderBase;
my $newIniFile = "$dir/$decoderBase.ini";
my $inputFileName = "$dir/input";
my $user = $ENV{"USER"};

# process ini file
-e $iniFile || die "Error: could not open $iniFile for reading\n";
open(INI, $iniFile);

if ($dryrun){
	write_config(*STDERR);
	exit 0;
} else {
	if (-e $dir){
	  die "ERROR: working dir $dir already exists\n\n";
	} else {
		mkdir $dir;
		mkdir "$dir/hgs";
    mkdir "$dir/scripts";
		unless (-e $initialWeights) {
			print STDERR "Please specify an initial weights file with --initial-weights\n";
			print_help();
			exit;
		}
		`cp $initialWeights $dir/weights.0`;
		die "Can't find weights.0" unless (-e "$dir/weights.0");
	}
	write_config(*STDERR);
}


# Generate initial files and values
`cp $iniFile $newIniFile`;
$iniFile = $newIniFile;

my $newsrc = "$dir/dev.input";
enseg($srcFile, $newsrc);
$srcFile = $newsrc;
my $devSize = 0;
open F, "<$srcFile" or die "Can't read $srcFile: $!";
while(<F>) { $devSize++; }
close F;

unless($best_weights){ $best_weights = $weights; }
unless($projected_score){ $projected_score = 0.0; }
$seen_weights{$weights} = 1;

my $random_seed = int(time / 1000);
my $lastWeightsFile;
my $lastPScore = 0;
# main optimization loop
while (1){
	print STDERR "\n\nITERATION $iteration\n==========\n";

	# iteration-specific files
	my $runFile="$dir/run.raw.$iteration";
	my $onebestFile="$dir/1best.$iteration";
	my $logdir="$dir/logs.$iteration";
	my $decoderLog="$logdir/decoder.sentserver.log.$iteration";
	my $scorerLog="$logdir/scorer.log.$iteration";
	`mkdir -p $logdir`;

	#decode
	print STDERR "DECODE\n";
	print STDERR `date`;
	my $im1 = $iteration - 1;
	my $weightsFile="$dir/weights.$im1";
	my $decoder_cmd = "$decoder -c $iniFile -w $weightsFile -O $dir/hgs";
	my $pcmd = "cat $srcFile | $parallelize -p $pmem -e $logdir -j $decode_nodes -- ";
        if ($run_local) { $pcmd = "cat $srcFile |"; }
        my $cmd = $pcmd . "$decoder_cmd 2> $decoderLog 1> $runFile";
	print STDERR "COMMAND:\n$cmd\n";
	my $result = 0;
	$result = system($cmd);
	unless ($result == 0){
		cleanup();
		print STDERR "ERROR: Parallel decoder returned non-zero exit code $result\n";
		die;
	}
	my $dec_score = `cat $runFile | $SCORER $refs_comma_sep -l $metric`;
	chomp $dec_score;
	print STDERR "DECODER SCORE: $dec_score\n";

	# save space
	`gzip $runFile`;
	`gzip $decoderLog`;

	if ($iteration > $max_iterations){
		print STDERR "\nREACHED STOPPING CRITERION: Maximum iterations\n";
		last;
	}
	
	# run optimizer
	print STDERR `date`;
	my $mergeLog="$logdir/prune-merge.log.$iteration";

	my $score = 0;
	my $icc = 0;
	my $inweights="$dir/weights.$im1";
	for (my $opt_iter=1; $opt_iter<$optimization_iters; $opt_iter++) {
		print STDERR "\nGENERATE OPTIMIZATION STRATEGY (OPT-ITERATION $opt_iter/$optimization_iters)\n";
		print STDERR `date`;
		$icc++;
		$cmd="$MAPINPUT -w $inweights -r $dir/hgs -s $devSize -d $rand_directions > $dir/agenda.$im1-$opt_iter";
		print STDERR "COMMAND:\n$cmd\n";
		$result = system($cmd);
		unless ($result == 0){
			cleanup();
			die "ERROR: mapinput command returned non-zero exit code $result\n";
		}

		`mkdir $dir/splag.$im1`;
		$cmd="split -a 3 -l $lines_per_mapper $dir/agenda.$im1-$opt_iter $dir/splag.$im1/mapinput.";
		print STDERR "COMMAND:\n$cmd\n";
		$result = system($cmd);
		unless ($result == 0){
			cleanup();
			print STDERR "ERROR: split command returned non-zero exit code $result\n";
			die;
		}
		opendir(DIR, "$dir/splag.$im1") or die "Can't open directory: $!";
		my @shards = grep { /^mapinput\./ } readdir(DIR);
		closedir DIR;
		die "No shards!" unless scalar @shards > 0;
		my $joblist = "";
		my $nmappers = 0;
		my @mapoutputs = ();
		@cleanupcmds = ();
		my %o2i = ();
		my $first_shard = 1;
		for my $shard (@shards) {
			my $mapoutput = $shard;
			my $client_name = $shard;
			$client_name =~ s/mapinput.//;
			$client_name = "vest.$client_name";
			$mapoutput =~ s/mapinput/mapoutput/;
			push @mapoutputs, "$dir/splag.$im1/$mapoutput";
			$o2i{"$dir/splag.$im1/$mapoutput"} = "$dir/splag.$im1/$shard";
			my $script = "$MAPPER -s $srcFile -l $metric $refs_comma_sep < $dir/splag.$im1/$shard | sort -t \$'\\t' -k 1 > $dir/splag.$im1/$mapoutput";
			if ($run_local) {
				print STDERR "COMMAND:\n$script\n";
				$result = system($script);
				unless ($result == 0){
					cleanup();
					die "ERROR: mapper returned non-zero exit code $result\n";
				}
			} else {
        my $script_file = "$dir/scripts/map.$shard";
        open F, ">$script_file" or die "Can't write $script_file: $!";
        print F "$script\n";
        close F;
				if ($first_shard) { print STDERR "$script\n"; $first_shard=0; }

				$nmappers++;
				my $jobid = `qsub $QSUB_FLAGS -S /bin/bash -N $client_name -o /dev/null -e $logdir/$client_name.ER $script_file`;
        die "qsub failed: $!" unless $? == 0;
	  		chomp $jobid;
				$jobid =~ s/^(\d+)(.*?)$/\1/g;
        $jobid =~ s/^Your job (\d+) .*$/\1/;
		  	push(@cleanupcmds, "`qdel $jobid 2> /dev/null`");
			  print STDERR " $jobid";
				if ($joblist == "") { $joblist = $jobid; }
				else {$joblist = $joblist . "\|" . $jobid; }
			}
		}
		if ($run_local) {
		} else {
			print STDERR "\nLaunched $nmappers mappers.\n";
      sleep 10;
			print STDERR "Waiting for mappers to complete...\n";
			while ($nmappers > 0) {
			  sleep 5;
			  my @livejobs = grep(/$joblist/, split(/\n/, `qstat`));
			  $nmappers = scalar @livejobs;
			}
			print STDERR "All mappers complete.\n";
		}
		my $tol = 0;
		my $til = 0;
		for my $mo (@mapoutputs) {
		  my $olines = get_lines($mo);
		  my $ilines = get_lines($o2i{$mo});
		  $tol += $olines;
		  $til += $ilines;
		  die "$mo: output lines ($olines) doesn't match input lines ($ilines)" unless $olines==$ilines;
		}
		print STDERR "Results for $tol/$til lines\n";
		print STDERR "\nSORTING AND RUNNING VEST REDUCER\n";
		print STDERR `date`;
		$cmd="sort -t \$'\\t' -k 1 @mapoutputs | $REDUCER -l $metric > $dir/redoutput.$im1";
		print STDERR "COMMAND:\n$cmd\n";
		$result = system($cmd);
		unless ($result == 0){
			cleanup();
			die "ERROR: reducer command returned non-zero exit code $result\n";
		}
		$cmd="sort -nk3 $DIR_FLAG '-t|' $dir/redoutput.$im1 | head -1";
		my $best=`$cmd`; chomp $best;
		print STDERR "$best\n";
		my ($oa, $x, $xscore) = split /\|/, $best;
		$score = $xscore;
		print STDERR "PROJECTED SCORE: $score\n";
		if (abs($x) < $epsilon) {
			print STDERR "\nOPTIMIZER: no score improvement: abs($x) < $epsilon\n";
			last;
		}
                my $psd = $score - $last_score;
                $last_score = $score;
		if (abs($psd) < $epsilon) {
			print STDERR "\nOPTIMIZER: no score improvement: abs($psd) < $epsilon\n";
			last;
		}
		my ($origin, $axis) = split /\s+/, $oa;

		my %ori = convert($origin);
		my %axi = convert($axis);

		my $finalFile="$dir/weights.$im1-$opt_iter";
		open W, ">$finalFile" or die "Can't write: $finalFile: $!";
                my $norm = 0;
		for my $k (sort keys %ori) {
			my $dd = $ori{$k} + $axi{$k} * $x;
                        $norm += $dd * $dd;
		}
                $norm = sqrt($norm);
		$norm = 1;
		for my $k (sort keys %ori) {
			my $v = ($ori{$k} + $axi{$k} * $x) / $norm;
			print W "$k $v\n";
		}
		`rm -rf $dir/splag.$im1`;
		$inweights = $finalFile;
	}
	$lastWeightsFile = "$dir/weights.$iteration";
	`cp $inweights $lastWeightsFile`;
	if ($icc < 2) {
		print STDERR "\nREACHED STOPPING CRITERION: score change too little\n";
		last;
	}
	$lastPScore = $score;
	$iteration++;
	print STDERR "\n==========\n";
}

print STDERR "\nFINAL WEIGHTS: $lastWeightsFile\n(Use -w <this file> with the decoder)\n\n";

print STDOUT "$lastWeightsFile\n";

exit 0;

sub normalize_weights {
  my ($rfn, $rpts, $feat) = @_;
  my @feat_names = @$rfn;
  my @pts = @$rpts;
  my $z = 1.0;
  for (my $i=0; $i < scalar @feat_names; $i++) {
    if ($feat_names[$i] eq $feat) {
      $z = $pts[$i];
      last;
    }
  }
  for (my $i=0; $i < scalar @feat_names; $i++) {
    $pts[$i] /= $z;
  }
  print STDERR " NORM WEIGHTS: @pts\n";
  return @pts;
}

sub get_lines {
  my $fn = shift @_;
  open FL, "<$fn" or die "Couldn't read $fn: $!";
  my $lc = 0;
  while(<FL>) { $lc++; }
  return $lc;
}

sub get_comma_sep_refs {
  my ($p) = @_;
  my $o = `echo $p`;
  chomp $o;
  my @files = split /\s+/, $o;
  return "-r " . join(' -r ', @files);
}

sub read_weights_file {
  my ($file) = @_;
  open F, "<$file" or die "Couldn't read $file: $!";
  my @r = ();
  my $pm = -1;
  while(<F>) {
    next if /^#/;
    next if /^\s*$/;
    chomp;
    if (/^(.+)\s+(.+)$/) {
      my $m = $1;
      my $w = $2;
      die "Weights out of order: $m <= $pm" unless $m > $pm;
      push @r, $w;
    } else {
      warn "Unexpected feature name in weight file: $_";
    }
  }
  close F;
  return join ' ', @r;
}

# subs
sub write_config {
	my $fh = shift;
	my $cleanup = "yes";
	if ($disable_clean) {$cleanup = "no";}

	print $fh "\n";
	print $fh "DECODER:          $decoder\n";
	print $fh "INI FILE:         $iniFile\n";
	print $fh "WORKING DIR:      $dir\n";
	print $fh "SOURCE (DEV):     $srcFile\n";
	print $fh "REFS (DEV):       $refFiles\n";
	print $fh "EVAL METRIC:      $metric\n";
	print $fh "START ITERATION:  $iteration\n";
	print $fh "MAX ITERATIONS:   $max_iterations\n";
	print $fh "DECODE NODES:     $decode_nodes\n";
	print $fh "HEAD NODE:        $host\n";
	print $fh "PMEM (DECODING):  $pmem\n";
	print $fh "CLEANUP:          $cleanup\n";
	print $fh "INITIAL WEIGHTS:  $initialWeights\n";
}

sub update_weights_file {
  my ($neww, $rfn, $rpts) = @_;
  my @feats = @$rfn;
  my @pts = @$rpts;
  my $num_feats = scalar @feats;
  my $num_pts = scalar @pts;
  die "$num_feats (num_feats) != $num_pts (num_pts)" unless $num_feats == $num_pts;
  open G, ">$neww" or die;
  for (my $i = 0; $i < $num_feats; $i++) {
    my $f = $feats[$i];
    my $lambda = $pts[$i];
    print G "$f $lambda\n";
  }
  close G;
}

sub enseg {
	my $src = shift;
	my $newsrc = shift;
	open(SRC, $src);
	open(NEWSRC, ">$newsrc");
	my $i=0;
	while (my $line=<SRC>){
		chomp $line;
		print NEWSRC "<seg id=\"$i\">$line</seg>\n";
		$i++;
	}
	close SRC;
	close NEWSRC;
}

sub print_help {

	my $executable = `basename $0`; chomp $executable;
    print << "Help";

Usage: $executable [options] <ini file>

	$executable [options] <ini file> 
		Runs a complete MERT optimization and test set decoding, using 
		the decoder configuration in ini file.  Note that many of the
		options have default values that are inferred automatically 
		based on certain conventions.  For details, refer to descriptions
		of the options --decoder, --weights, and --workdir. 

Options:

	--local
		Run the decoder and optimizer locally.

	--decoder <decoder path>
		Decoder binary to use.

	--help
		Print this message and exit.

	--iteration <I> 
		Starting iteration number.  If not specified, defaults to 1.

	--max-iterations <M> 
		Maximum number of iterations to run.  If not specified, defaults
		to 10.

	--pmem <N>
		Amount of physical memory requested for parallel decoding jobs.

	--ref-files <files> 
		Dev set ref files.  This option takes only a single string argument. 
		To use multiple files (including file globbing), this argument should 
		be quoted.

	--metric <method>
		Metric to optimize.
		Example values: IBM_BLEU, NIST_BLEU, Koehn_BLEU, TER, Combi

	--normalize <feature-name>
		After each iteration, rescale all feature weights such that feature-
		name has a weight of 1.0.

	--rand-directions <num>
		MERT will attempt to optimize along all of the principle directions,
		set this parameter to explore other directions. Defaults to 5.

	--source-file <file> 
		Dev set source file.

	--weights <file> 
		A file specifying initial feature weights.  The format is
		FeatureName_1 value1
		FeatureName_2 value2

	--workdir <dir> 
		Directory for intermediate and output files.  If not specified, the
		name is derived from the ini filename.  Assuming that the ini 
		filename begins with the decoder name and ends with ini, the default
		name of the working directory is inferred from the middle part of 
		the filename.  E.g. an ini file named decoder.foo.ini would have
		a default working directory name foo.

Help
}

sub convert {
  my ($str) = @_;
  my @ps = split /;/, $str;
  my %dict = ();
  for my $p (@ps) {
    my ($k, $v) = split /=/, $p;
    $dict{$k} = $v;
  }
  return %dict;
}


