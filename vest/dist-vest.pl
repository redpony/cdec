#!/usr/bin/env perl

use strict;
my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }
use Getopt::Long;
use IPC::Open2;
use strict;
use POSIX ":sys_wait_h";

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
my $SCORER = $FAST_SCORE;
die "Can't find $MAPPER" unless -x $MAPPER;
my $cdec = "$bin_dir/../decoder/cdec";
die "Can't find decoder in $cdec" unless -x $cdec;
my $decoder = $cdec;
my $DISCARD_FORESTS = 0;
my $lines_per_mapper = 400;
my $rand_directions = 15;
my $iteration = 1;
my $run_local = 0;
my $best_weights;
my $max_iterations = 15;
my $optimization_iters = 6;
my $num_rand_points = 20;
my $mert_nodes = join(" ", grep(/^c\d\d$/, split(/\n/, `pbsnodes -a`))); # "1 1 1 1 1" fails due to file staging conflicts
my $decode_nodes = "1 1 1 1 1 1 1 1 1 1 1 1 1 1 1";  # start 15 jobs
my $pmem = "3g";
my $disable_clean = 0;
my %seen_weights;
my $normalize;
my $help = 0;
my $epsilon = 0.0001;
my $interval = 5;
my $dryrun = 0;
my $ranges;
my $last_score = -10000000;
my $restart = 0;
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
	"decode-nodes=s" => \$decode_nodes,
	"dont-clean" => \$disable_clean,
	"dry-run" => \$dryrun,
	"epsilon" => \$epsilon,
	"help" => \$help,
	"interval" => \$interval,
	"iteration=i" => \$iteration,
	"local" => \$run_local,
	"max-iterations=i" => \$max_iterations,
	"mert-nodes=s" => \$mert_nodes,
	"normalize=s" => \$normalize,
	"pmem=s" => \$pmem,
	"ranges=s" => \$ranges,
	"rand-directions=i" => \$rand_directions,
	"ref-files=s" => \$refFiles,
	"metric=s" => \$metric,
	"restart" => \$restart,
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
if ($restart){
	$iniFile = `ls $dir/*.ini`; chomp $iniFile;
	unless (-e $iniFile){
		die "ERROR: Could not find ini file in $dir to restart\n";
	}
	$logfile = "$dir/mert.log";
	open(LOGFILE, ">>$logfile");
	print LOGFILE "RESTARTING STOPPED OPTIMIZATION\n\n";

	# figure out best weights so far and iteration number
	open(LOG, "$dir/mert.log");
	my $wi = 0;
	while (my $line = <LOG>){
		chomp $line;
		if ($line =~ /ITERATION (\d+)/) {
			$iteration = $1;
		}
	}

	$iteration = $wi + 1;
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
my $parallelize = '/chomes/redpony/svn-trunk/sa-utils/parallelize.pl';
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
		unless($restart){
			die "ERROR: working dir $dir already exists\n\n";
		}
	} else {
		mkdir $dir;
		mkdir "$dir/hgs";
		unless (-e $initialWeights) {
			print STDERR "Please specify an initial weights file with --initial-weights\n";
			print_help();
			exit;
		}
		`cp $initialWeights $dir/weights.0`;
		die "Can't find weights.0" unless (-e "$dir/weights.0");
	}
	unless($restart){
		$logfile = "$dir/mert.log";
		open(LOGFILE, ">$logfile");
	}
	write_config(*LOGFILE);
}


# Generate initial files and values
unless ($restart){ `cp $iniFile $newIniFile`; }
$iniFile = $newIniFile;

my $newsrc = "$dir/dev.input";
unless($restart){ enseg($srcFile, $newsrc); }
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
	print LOGFILE "\n\nITERATION $iteration\n==========\n";

	# iteration-specific files
	my $runFile="$dir/run.raw.$iteration";
	my $onebestFile="$dir/1best.$iteration";
	my $logdir="$dir/logs.$iteration";
	my $decoderLog="$logdir/decoder.sentserver.log.$iteration";
	my $scorerLog="$logdir/scorer.log.$iteration";
	`mkdir -p $logdir`;

	#decode
	print LOGFILE "DECODE\n";
	print LOGFILE `date`;
	my $im1 = $iteration - 1;
	my $weightsFile="$dir/weights.$im1";
	my $decoder_cmd = "$decoder -c $iniFile -w $weightsFile -O $dir/hgs";
	my $pcmd = "cat $srcFile | $parallelize -p $pmem -e $logdir -n \"$decode_nodes\" -- ";
        if ($run_local) { $pcmd = "cat $srcFile |"; }
        my $cmd = $pcmd . "$decoder_cmd 2> $decoderLog 1> $runFile";
	print LOGFILE "COMMAND:\n$cmd\n";
	my $result = 0;
	$result = system($cmd);
	unless ($result == 0){
		cleanup();
		print LOGFILE "ERROR: Parallel decoder returned non-zero exit code $result\n";
		die;
	}
	my $dec_score = `cat $runFile | $SCORER $refs_comma_sep -l $metric`;
	chomp $dec_score;
	print LOGFILE "DECODER SCORE: $dec_score\n";

	# save space
	`gzip $runFile`;
	`gzip $decoderLog`;

	if ($iteration > $max_iterations){
		print LOGFILE "\nREACHED STOPPING CRITERION: Maximum iterations\n";
		last;
	}
	
	# run optimizer
	print LOGFILE "\nUNION FORESTS\n";
	print LOGFILE `date`;
	my $mergeLog="$logdir/prune-merge.log.$iteration";
	if ($DISCARD_FORESTS) {
		`rm -f $dir/hgs/*gz`;
	}

	my $score = 0;
	my $icc = 0;
	my $inweights="$dir/weights.$im1";
	for (my $opt_iter=1; $opt_iter<$optimization_iters; $opt_iter++) {
		print LOGFILE "\nGENERATE OPTIMIZATION STRATEGY (OPT-ITERATION $opt_iter/$optimization_iters)\n";
		print LOGFILE `date`;
		$icc++;
		$cmd="$MAPINPUT -w $inweights -r $dir/hgs -s $devSize -d $rand_directions > $dir/agenda.$im1-$opt_iter";
		print LOGFILE "COMMAND:\n$cmd\n";
		$result = system($cmd);
		unless ($result == 0){
			cleanup();
			print LOGFILE "ERROR: mapinput command returned non-zero exit code $result\n";
			die;
		}

	        `mkdir $dir/splag.$im1`;
		$cmd="split -a 3 -l $lines_per_mapper $dir/agenda.$im1-$opt_iter $dir/splag.$im1/mapinput.";
		print LOGFILE "COMMAND:\n$cmd\n";
		$result = system($cmd);
		unless ($result == 0){
			cleanup();
			print LOGFILE "ERROR: split command returned non-zero exit code $result\n";
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
				print LOGFILE "COMMAND:\n$script\n";
				$result = system($script);
				unless ($result == 0){
					cleanup();
					print LOGFILE "ERROR: mapper returned non-zero exit code $result\n";
					die;
				}
			} else {
				my $todo = "qsub -q batch -l pmem=3000mb,walltime=5:00:00 -N $client_name -o /dev/null -e $logdir/$client_name.ER";
				local(*QOUT, *QIN);
				open2(\*QOUT, \*QIN, $todo);
				print QIN $script;
				if ($first_shard) { print LOGFILE "$script\n"; $first_shard=0; }
				close QIN;
				$nmappers++;
				while (my $jobid=<QOUT>){
	  				chomp $jobid;
		  			push(@cleanupcmds, "`qdel $jobid 2> /dev/null`");
					$jobid =~ s/^(\d+)(.*?)$/\1/g;
					print STDERR "short job id $jobid\n";
					if ($joblist == "") { $joblist = $jobid; }
					else {$joblist = $joblist . "\|" . $jobid; }
				}
				close QOUT;
			}
		}
		if ($run_local) {
		} else {
			print LOGFILE "Launched $nmappers mappers.\n";
			print LOGFILE "Waiting for mappers to complete...\n";
			while ($nmappers > 0) {
			  sleep 5;
			  my @livejobs = grep(/$joblist/, split(/\n/, `qstat`));
			  $nmappers = scalar @livejobs;
			}
			print LOGFILE "All mappers complete.\n";
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
		print LOGFILE "Results for $tol/$til lines\n";
		print LOGFILE "\nSORTING AND RUNNING FMERT REDUCER\n";
		print LOGFILE `date`;
		$cmd="sort -t \$'\\t' -k 1 @mapoutputs | $REDUCER -l $metric > $dir/redoutput.$im1";
		print LOGFILE "COMMAND:\n$cmd\n";
		$result = system($cmd);
		unless ($result == 0){
			cleanup();
			print LOGFILE "ERROR: reducer command returned non-zero exit code $result\n";
			die;
		}
		$cmd="sort -nk3 $DIR_FLAG '-t|' $dir/redoutput.$im1 | head -1";
		my $best=`$cmd`; chomp $best;
		print LOGFILE "$best\n";
		my ($oa, $x, $xscore) = split /\|/, $best;
		$score = $xscore;
		print LOGFILE "PROJECTED SCORE: $score\n";
		if (abs($x) < $epsilon) {
			print LOGFILE "\nOPTIMIZER: no score improvement: abs($x) < $epsilon\n";
			last;
		}
                my $psd = $score - $last_score;
                $last_score = $score;
		if (abs($psd) < $epsilon) {
			print LOGFILE "\nOPTIMIZER: no score improvement: abs($psd) < $epsilon\n";
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
		print LOGFILE "\nREACHED STOPPING CRITERION: score change too little\n";
		last;
	}
	$lastPScore = $score;
	$iteration++;
	print LOGFILE "\n==========\n";
}

print LOGFILE "\nFINAL WEIGHTS: $dir/$lastWeightsFile\n(Use -w <this file> with hiero)\n\n";

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
  print LOGFILE " NORM WEIGHTS: @pts\n";
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
	print $fh "MERT NODES:       $mert_nodes\n";
	print $fh "DECODE NODES:     $decode_nodes\n";
	print $fh "HEAD NODE:        $host\n";
	print $fh "PMEM (DECODING):  $pmem\n";
	print $fh "CLEANUP:          $cleanup\n";
	print $fh "INITIAL WEIGHTS:  $initialWeights\n";

	if ($restart){
		print $fh "PROJECTED BLEU:   $projected_score\n";
		print $fh "BEST WEIGHTS:     $best_weights\n";
	}
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
       $executable --restart <work dir>

	$executable [options] <ini file> 
		Runs a complete MERT optimization and test set decoding, using 
		the decoder configuration in ini file.  Note that many of the
		options have default values that are inferred automatically 
		based on certain conventions.  For details, refer to descriptions
		of the options --decoder, --weights, and --workdir. 

	$executable --restart <work dir>
		Continues an optimization run that was stopped for some reason,
		using configuration information found in the working directory
		left behind by the stopped run.

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


