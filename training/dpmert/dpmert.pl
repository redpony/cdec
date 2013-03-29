#!/usr/bin/env perl
use strict;
my @ORIG_ARGV=@ARGV;
use Cwd qw(getcwd);
my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR, "$SCRIPT_DIR/../../environment", "$SCRIPT_DIR/../utils"; }

# Skip local config (used for distributing jobs) if we're running in local-only mode
use LocalConfig;
use Getopt::Long;
use File::Basename qw(basename);
require "libcall.pl";

my $QSUB_CMD = qsub_args(mert_memory());

# Default settings
my $srcFile;  # deprecated
my $refFiles; # deprecated
my $default_jobs = env_default_jobs();
my $bin_dir = $SCRIPT_DIR;
my $util_dir = "$SCRIPT_DIR/../utils";
die "Bin directory $bin_dir missing/inaccessible" unless -d $bin_dir;
my $FAST_SCORE="$bin_dir/../../mteval/fast_score";
die "Can't execute $FAST_SCORE" unless -x $FAST_SCORE;
my $MAPINPUT = "$bin_dir/mr_dpmert_generate_mapper_input";
my $MAPPER = "$bin_dir/mr_dpmert_map";
my $REDUCER = "$bin_dir/mr_dpmert_reduce";
my $parallelize = "$util_dir/parallelize.pl";
my $libcall = "$util_dir/libcall.pl";
my $sentserver = "$util_dir/sentserver";
my $sentclient = "$util_dir/sentclient";
my $LocalConfig = "$SCRIPT_DIR/../../environment/LocalConfig.pm";

my $SCORER = $FAST_SCORE;
die "Can't find $MAPPER" unless -x $MAPPER;
my $cdec = "$bin_dir/../../decoder/cdec";
die "Can't find decoder in $cdec" unless -x $cdec;
die "Can't find $parallelize" unless -x $parallelize;
die "Can't find $libcall" unless -e $libcall;
my $decoder = $cdec;
my $lines_per_mapper = 200;
my $rand_directions = 15;
my $iteration = 1;
my $best_weights;
my $max_iterations = 15;
my $optimization_iters = 6;
my $jobs = $default_jobs;   # number of decode nodes
my $pmem = "9g";
my $disable_clean = 0;
my %seen_weights;
my $help = 0;
my $epsilon = 0.0001;
my $last_score = -10000000;
my $metric = "ibm_bleu";
my $dir;
my $iniFile;
my $weights;
my $initialWeights;
my $bleu_weight=1;
my $use_make = 1;  # use make to parallelize line search
my $useqsub;
my $pass_suffix = '';
my $devset;
# Process command-line options
if (GetOptions(
	"config=s" => \$iniFile,
	"weights=s" => \$initialWeights,
        "devset=s" => \$devset,
	"jobs=i" => \$jobs,
	"pass-suffix=s" => \$pass_suffix,
	"help" => \$help,
	"qsub" => \$useqsub,
	"iterations=i" => \$max_iterations,
	"pmem=s" => \$pmem,
	"random-directions=i" => \$rand_directions,
	"metric=s" => \$metric,
	"source-file=s" => \$srcFile,
	"output-dir=s" => \$dir,
) == 0 || @ARGV!=0 || $help) {
	print_help();
	exit;
}

if ($useqsub) {
  $use_make = 0;
  die "LocalEnvironment.pm does not have qsub configuration for this host. Cannot run with --qsub!\n" unless has_qsub();
}

my @missing_args = ();
if (defined $srcFile || defined $refFiles) {
  die <<EOT;

  The options --ref-files and --source-file are no longer supported.
  Please specify the input file and its reference translations with
  --devset FILE

EOT
}

if (!defined $iniFile) { push @missing_args, "--config"; }
if (!defined $devset) { push @missing_args, "--devset"; }
if (!defined $initialWeights) { push @missing_args, "--weights"; }
die "Please specify missing arguments: " . join (', ', @missing_args) . "\nUse --help for more information.\n" if (@missing_args);

if ($metric =~ /^(combi|ter)$/i) {
  $lines_per_mapper = 40;
} elsif ($metric =~ /^meteor$/i) {
  $lines_per_mapper = 2000;   # start up time is really high for METEOR
}


my $nodelist;
my $host =check_output("hostname"); chomp $host;
my $bleu;
my $interval_count = 0;
my $logfile;
my $projected_score;

# used in sorting scores
my $DIR_FLAG = '-r';
if ($metric =~ /^ter$|^aer$/i) {
  $DIR_FLAG = '';
}

unless ($dir){
	$dir = "dpmert";
}
unless ($dir =~ /^\//){  # convert relative path to absolute path
	my $basedir = check_output("pwd");
	chomp $basedir;
	$dir = "$basedir/$dir";
}


# Initializations and helper functions
srand;

my @childpids = ();
my @cleanupcmds = ();

sub cleanup {
	print STDERR "Cleanup...\n";
	for my $pid (@childpids){ unchecked_call("kill $pid"); }
	for my $cmd (@cleanupcmds){ unchecked_call("$cmd"); }
	exit 1;
};
# Always call cleanup, no matter how we exit
*CORE::GLOBAL::exit = sub{ cleanup(); }; 
$SIG{INT} = "cleanup";
$SIG{TERM} = "cleanup";
$SIG{HUP} = "cleanup";

my $decoderBase = basename($decoder); chomp $decoderBase;
my $newIniFile = "$dir/$decoderBase.ini";
my $inputFileName = "$dir/input";
my $user = $ENV{"USER"};

# process ini file
-e $iniFile || die "Error: could not open $iniFile for reading\n";

sub dirsize {
    opendir ISEMPTY,$_[0];
    return scalar(readdir(ISEMPTY))-1;
}
if (-e $dir) {
	# allow preexisting logfile, binaries, but not dist-dpmert.pl outputs
	die "ERROR: output directory $dir already exists (remove or use --output-dir dir)\n\n";
} else {
	mkdir "$dir" or die "Can't mkdir $dir: $!";
	mkdir "$dir/hgs" or die;
	mkdir "$dir/scripts" or die;
	print STDERR <<EOT;
	DECODER:          $decoder
	INI FILE:         $iniFile
	WORKING DIR:      $dir
	DEVSET:           $devset
	EVAL METRIC:      $metric
	MAX ITERATIONS:   $max_iterations
	PARALLEL JOBS:    $jobs
	HEAD NODE:        $host
	PMEM (DECODING):  $pmem
	INITIAL WEIGHTS:  $initialWeights
EOT
}

# Generate initial files and values
check_call("cp $iniFile $newIniFile");
check_call("cp $initialWeights $dir/weights.0");
$iniFile = $newIniFile;

split_devset($devset, "$dir/dev.input.raw", "$dir/dev.refs");
my $refs = "-r $dir/dev.refs";
my $newsrc = "$dir/dev.input";
enseg("$dir/dev.input.raw", $newsrc);
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

	if ($iteration > $max_iterations){
		print STDERR "\nREACHED STOPPING CRITERION: Maximum iterations\n";
		last;
	}
	# iteration-specific files
	my $runFile="$dir/run.raw.$iteration";
	my $onebestFile="$dir/1best.$iteration";
	my $logdir="$dir/logs.$iteration";
	my $decoderLog="$logdir/decoder.sentserver.log.$iteration";
	my $scorerLog="$logdir/scorer.log.$iteration";
	check_call("mkdir -p $logdir");


	#decode
	print STDERR "RUNNING DECODER AT ";
	print STDERR unchecked_output("date");
	my $im1 = $iteration - 1;
	my $weightsFile="$dir/weights.$im1";
	my $decoder_cmd = "$decoder -c $iniFile --weights$pass_suffix $weightsFile -O $dir/hgs";
	my $pcmd;
	if ($use_make) {
		$pcmd = "cat $srcFile | $parallelize --workdir $dir --use-fork -p $pmem -e $logdir -j $jobs --";
	} else {
		$pcmd = "cat $srcFile | $parallelize --workdir $dir -p $pmem -e $logdir -j $jobs --";
	}
	my $cmd = "$pcmd $decoder_cmd 2> $decoderLog 1> $runFile";
	print STDERR "COMMAND:\n$cmd\n";
	check_bash_call($cmd);
        my $num_hgs;
        my $num_topbest;
        my $retries = 0;
	while($retries < 5) {
	    $num_hgs = check_output("ls $dir/hgs/*.gz | wc -l");
	    $num_topbest = check_output("wc -l < $runFile");
	    print STDERR "NUMBER OF HGs: $num_hgs\n";
	    print STDERR "NUMBER OF TOP-BEST HYPs: $num_topbest\n";
	    if($devSize == $num_hgs && $devSize == $num_topbest) {
		last;
	    } else {
		print STDERR "Incorrect number of hypergraphs or topbest. Waiting for distributed filesystem and retrying...\n";
		sleep(3);
	    }
	    $retries++;
	}
	die "Dev set contains $devSize sentences, but we don't have topbest and hypergraphs for all these! Decoder failure? Check $decoderLog\n" if ($devSize != $num_hgs || $devSize != $num_topbest);
	my $dec_score = check_output("cat $runFile | $SCORER $refs -m $metric");
	chomp $dec_score;
	print STDERR "DECODER SCORE: $dec_score\n";

	# save space
	check_call("gzip -f $runFile");
	check_call("gzip -f $decoderLog");

	# run optimizer
	print STDERR "RUNNING OPTIMIZER AT ";
	print STDERR unchecked_output("date");
	my $mergeLog="$logdir/prune-merge.log.$iteration";

	my $score = 0;
	my $icc = 0;
	my $inweights="$dir/weights.$im1";
	for (my $opt_iter=1; $opt_iter<$optimization_iters; $opt_iter++) {
		print STDERR "\nGENERATE OPTIMIZATION STRATEGY (OPT-ITERATION $opt_iter/$optimization_iters)\n";
		print STDERR unchecked_output("date");
		$icc++;
		$cmd="$MAPINPUT -w $inweights -r $dir/hgs -s $devSize -d $rand_directions > $dir/agenda.$im1-$opt_iter";
		print STDERR "COMMAND:\n$cmd\n";
		check_call($cmd);
		check_call("mkdir -p $dir/splag.$im1");
		$cmd="split -a 3 -l $lines_per_mapper $dir/agenda.$im1-$opt_iter $dir/splag.$im1/mapinput.";
		print STDERR "COMMAND:\n$cmd\n";
		check_call($cmd);
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
		my $mkfile; # only used with makefiles
		my $mkfilename;
		if ($use_make) {
			$mkfilename = "$dir/splag.$im1/domap.mk";
			open $mkfile, ">$mkfilename" or die "Couldn't write $mkfilename: $!";
			print $mkfile "all: $dir/splag.$im1/map.done\n\n";
		}
		my @mkouts = ();  # only used with makefiles
		for my $shard (@shards) {
			my $mapoutput = $shard;
			my $client_name = $shard;
			$client_name =~ s/mapinput.//;
			$client_name = "dpmert.$client_name";
			$mapoutput =~ s/mapinput/mapoutput/;
			push @mapoutputs, "$dir/splag.$im1/$mapoutput";
			$o2i{"$dir/splag.$im1/$mapoutput"} = "$dir/splag.$im1/$shard";
			my $script = "$MAPPER -s $srcFile -m $metric $refs < $dir/splag.$im1/$shard | sort -t \$'\\t' -k 1 > $dir/splag.$im1/$mapoutput";
			if ($use_make) {
				my $script_file = "$dir/scripts/map.$shard";
				open F, ">$script_file" or die "Can't write $script_file: $!";
				print F "#!/bin/bash\n";
				print F "$script\n";
				close F;
				my $output = "$dir/splag.$im1/$mapoutput";
				push @mkouts, $output;
				chmod(0755, $script_file) or die "Can't chmod $script_file: $!";
				if ($first_shard) { print STDERR "$script\n"; $first_shard=0; }
				print $mkfile "$output: $dir/splag.$im1/$shard\n\t$script_file\n\n";
			} else {
				my $script_file = "$dir/scripts/map.$shard";
				open F, ">$script_file" or die "Can't write $script_file: $!";
				print F "$script\n";
				close F;
				if ($first_shard) { print STDERR "$script\n"; $first_shard=0; }

				$nmappers++;
				my $qcmd = "$QSUB_CMD -N $client_name -o /dev/null -e $logdir/$client_name.ER $script_file";
				my $jobid = check_output("$qcmd");
				chomp $jobid;
				$jobid =~ s/^(\d+)(.*?)$/\1/g;
				$jobid =~ s/^Your job (\d+) .*$/\1/;
		 	 	push(@cleanupcmds, "qdel $jobid 2> /dev/null");
				print STDERR " $jobid";
				if ($joblist == "") { $joblist = $jobid; }
				else {$joblist = $joblist . "\|" . $jobid; }
			}
		}
		if ($use_make) {
			print $mkfile "$dir/splag.$im1/map.done: @mkouts\n\ttouch $dir/splag.$im1/map.done\n\n";
			close $mkfile;
			my $mcmd = "make -j $jobs -f $mkfilename";
			print STDERR "\nExecuting: $mcmd\n";
			check_call($mcmd);
		} else {
			print STDERR "\nLaunched $nmappers mappers.\n";
      			sleep 8;
			print STDERR "Waiting for mappers to complete...\n";
			while ($nmappers > 0) {
			  sleep 5;
			  my @livejobs = grep(/$joblist/, split(/\n/, unchecked_output("qstat | grep -v ' C '")));
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
		print STDERR unchecked_output("date");
		$cmd="sort -t \$'\\t' -k 1 @mapoutputs | $REDUCER -m $metric > $dir/redoutput.$im1";
		print STDERR "COMMAND:\n$cmd\n";
		check_bash_call($cmd);
		$cmd="sort -nk3 $DIR_FLAG '-t|' $dir/redoutput.$im1 | head -1";
		# sort returns failure even when it doesn't fail for some reason
		my $best=unchecked_output("$cmd"); chomp $best;
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
		check_call("rm $dir/splag.$im1/*");
		$inweights = $finalFile;
	}
	$lastWeightsFile = "$dir/weights.$iteration";
	check_call("cp $inweights $lastWeightsFile");
	if ($icc < 2) {
		print STDERR "\nREACHED STOPPING CRITERION: score change too little\n";
		last;
	}
	$lastPScore = $score;
	$iteration++;
	print STDERR "\n==========\n";
}

check_call("cp $lastWeightsFile $dir/weights.final");
print STDERR "\nFINAL WEIGHTS: $dir/weights.final\n(Use -w <this file> with the decoder)\n\n";
print STDOUT "$dir/weights.final\n";
exit 0;


sub get_lines {
  my $fn = shift @_;
  open FL, "<$fn" or die "Couldn't read $fn: $!";
  my $lc = 0;
  while(<FL>) { $lc++; }
  return $lc;
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
		if ($line =~ /^\s*<seg/i) {
		    if($line =~ /id="[0-9]+"/) {
			print NEWSRC "$line\n";
		    } else {
			die "When using segments with pre-generated <seg> tags, you must include a zero-based id attribute";
		    }
		} else {
			print NEWSRC "<seg id=\"$i\">$line</seg>\n";
		}
		$i++;
	}
	close SRC;
	close NEWSRC;
}

sub print_help {

	my $executable = basename($0); chomp $executable;
	print << "Help";

Usage: $executable [options] <ini file>

	$executable [options]
		Runs a complete MERT optimization. Required options are --weights,
		--devset, and --config.

Options:

	--config <file>   [-c <file>]
		The decoder configuration file.

	--devset <file>   [-d <file>]
		The source *and* references for the development set.

	--weights <file>  [-w <file>]
		A file specifying initial feature weights.  The format is
		FeatureName_1 value1
		FeatureName_2 value2
		**All and only the weights listed in <file> will be optimized!**

	--metric <name>
		Metric to optimize.
		Example values: IBM_BLEU, NIST_BLEU, Koehn_BLEU, TER, Combi

	--iterations <M>
		Maximum number of iterations to run.  If not specified, defaults
		to 10.

	--pass-suffix <S>
		If the decoder is doing multi-pass decoding, the pass suffix "2",
		"3", etc., is used to control what iteration of weights is set.

	--rand-directions <num>
		MERT will attempt to optimize along all of the principle directions,
		set this parameter to explore other directions. Defaults to 5.

	--output-dir <dir>
		Directory for intermediate and output files.

	--help
		Print this message and exit.

Job control options:

	--jobs <I>
		Number of decoder processes to run in parallel. [default=$default_jobs]

	--qsub
		Use qsub to run jobs in parallel (qsub must be configured in
		environment/LocalEnvironment.pm)

	--pmem <N>
		Amount of physical memory requested for parallel decoding jobs
		(used with qsub requests only)

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



sub cmdline {
    return join ' ',($0,@ORIG_ARGV);
}

#buggy: last arg gets quoted sometimes?
my $is_shell_special=qr{[ \t\n\\><|&;"'`~*?{}$!()]};
my $shell_escape_in_quote=qr{[\\"\$`!]};

sub escape_shell {
    my ($arg)=@_;
    return undef unless defined $arg;
    if ($arg =~ /$is_shell_special/) {
        $arg =~ s/($shell_escape_in_quote)/\\$1/g;
        return "\"$arg\"";
    }
    return $arg;
}

sub escaped_shell_args {
    return map {local $_=$_;chomp;escape_shell($_)} @_;
}

sub escaped_shell_args_str {
    return join ' ',&escaped_shell_args(@_);
}

sub escaped_cmdline {
    return "$0 ".&escaped_shell_args_str(@ORIG_ARGV);
}

sub split_devset {
  my ($infile, $outsrc, $outref) = @_;
  open F, "<$infile" or die "Can't read $infile: $!";
  open S, ">$outsrc" or die "Can't write $outsrc: $!";
  open R, ">$outref" or die "Can't write $outref: $!";
  while(<F>) {
    chomp;
    my ($src, @refs) = split /\s*\|\|\|\s*/;
    die "Malformed devset line: $_\n" unless scalar @refs > 0;
    print S "$src\n";
    print R join(' ||| ', @refs) . "\n";
  }
  close R;
  close S;
  close F;
}

