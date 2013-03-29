#!/usr/bin/env perl
use strict;
my @ORIG_ARGV=@ARGV;
use Cwd qw(getcwd);
my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR, "$SCRIPT_DIR/../../environment", "$SCRIPT_DIR/../utils"; }

# Skip local config (used for distributing jobs) if we're running in local-only mode
use LocalConfig;
use Getopt::Long;
use IPC::Open2;
use POSIX ":sys_wait_h";
my $QSUB_CMD = qsub_args(mert_memory());
my $default_jobs = env_default_jobs();

my $UTILS_DIR="$SCRIPT_DIR/../utils";
require "$UTILS_DIR/libcall.pl";

# Default settings
my $srcFile;
my $refFiles;
my $bin_dir = $SCRIPT_DIR;
die "Bin directory $bin_dir missing/inaccessible" unless -d $bin_dir;
my $FAST_SCORE="$bin_dir/../../mteval/fast_score";
die "Can't execute $FAST_SCORE" unless -x $FAST_SCORE;
my $MAPINPUT = "$bin_dir/rampion_generate_input.pl";
my $MAPPER = "$bin_dir/rampion_cccp";
my $parallelize = "$UTILS_DIR/parallelize.pl";
my $libcall = "$UTILS_DIR/libcall.pl";
my $sentserver = "$UTILS_DIR/sentserver";
my $sentclient = "$UTILS_DIR/sentclient";
my $LocalConfig = "$SCRIPT_DIR/../../environment/LocalConfig.pm";

my $SCORER = $FAST_SCORE;
die "Can't find $MAPPER" unless -x $MAPPER;
my $cdec = "$bin_dir/../../decoder/cdec";
die "Can't find decoder in $cdec" unless -x $cdec;
die "Can't find $parallelize" unless -x $parallelize;
die "Can't find $libcall" unless -e $libcall;
my $decoder = $cdec;
my $lines_per_mapper = 30;
my $iteration = 1;
my $best_weights;
my $psi = 1;
my $default_max_iter = 30;
my $max_iterations = $default_max_iter;
my $jobs = $default_jobs;   # number of decode nodes
my $pmem = "4g";
my $disable_clean = 0;
my %seen_weights;
my $help = 0;
my $epsilon = 0.0001;
my $dryrun = 0;
my $last_score = -10000000;
my $metric = "ibm_bleu";
my $dir;
my $iniFile;
my $weights;
my $use_make = 1;  # use make to parallelize
my $useqsub = 0;
my $initial_weights;
my $pass_suffix = '';
my $cpbin=1;

# regularization strength
my $tune_regularizer = 0;
my $reg = 500;
my $reg_previous = 5000;
my $dont_accum = 0;

# Process command-line options
Getopt::Long::Configure("no_auto_abbrev");
if (GetOptions(
	"jobs=i" => \$jobs,
	"dont-clean" => \$disable_clean,
	"dont-accumulate" => \$dont_accum,
	"pass-suffix=s" => \$pass_suffix,
        "qsub" => \$useqsub,
	"dry-run" => \$dryrun,
	"epsilon=s" => \$epsilon,
	"help" => \$help,
        "weights=s" => \$initial_weights,
	"reg=f" => \$reg,
	"use-make=i" => \$use_make,
	"max-iterations=i" => \$max_iterations,
	"pmem=s" => \$pmem,
        "cpbin!" => \$cpbin,
	"ref-files=s" => \$refFiles,
	"metric=s" => \$metric,
	"source-file=s" => \$srcFile,
	"workdir=s" => \$dir,
) == 0 || @ARGV!=1 || $help) {
	print_help();
	exit;
}

die "--tune-regularizer is no longer supported with --reg-previous and --reg. Please tune manually.\n" if $tune_regularizer;

if ($useqsub) {
  $use_make = 0;
  die "LocalEnvironment.pm does not have qsub configuration for this host. Cannot run with --qsub!\n" unless has_qsub();
}

my @missing_args = ();
if (!defined $srcFile) { push @missing_args, "--source-file"; }
if (!defined $refFiles) { push @missing_args, "--ref-files"; }
if (!defined $initial_weights) { push @missing_args, "--weights"; }
die "Please specify missing arguments: " . join (', ', @missing_args) . "\n" if (@missing_args);

if ($metric =~ /^(combi|ter)$/i) {
  $lines_per_mapper = 5;
}

($iniFile) = @ARGV;


sub write_config;
sub enseg;
sub print_help;

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

my $refs_comma_sep = get_comma_sep_refs('r',$refFiles);

unless ($dir){
	$dir = "rampion";
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
*CORE::GLOBAL::exit = 
    sub{ cleanup(); }; 
$SIG{INT} = "cleanup";
$SIG{TERM} = "cleanup";
$SIG{HUP} = "cleanup";

my $decoderBase = check_output("basename $decoder"); chomp $decoderBase;
my $newIniFile = "$dir/$decoderBase.ini";
my $inputFileName = "$dir/input";
my $user = $ENV{"USER"};
# process ini file
-e $iniFile || die "Error: could not open $iniFile for reading\n";
open(INI, $iniFile);

use File::Basename qw(basename);
#pass bindir, refs to vars holding bin
sub modbin {
    local $_;
    my $bindir=shift;
    check_call("mkdir -p $bindir");
    -d $bindir || die "couldn't make bindir $bindir";
    for (@_) {
        my $src=$$_;
        $$_="$bindir/".basename($src);
        check_call("cp -p $src $$_");
    }
}
sub dirsize {
    opendir ISEMPTY,$_[0];
    return scalar(readdir(ISEMPTY))-1;
}
my @allweights;
if ($dryrun){
	write_config(*STDERR);
	exit 0;
} else {
	if (-e $dir && dirsize($dir)>1 && -e "$dir/hgs" ){ # allow preexisting logfile, binaries, but not dist-pro.pl outputs
	  die "ERROR: working dir $dir already exists\n\n";
	} else {
		-e $dir || mkdir $dir;
		mkdir "$dir/hgs";
        modbin("$dir/bin",\$LocalConfig,\$cdec,\$SCORER,\$MAPINPUT,\$MAPPER,\$parallelize,\$sentserver,\$sentclient,\$libcall) if $cpbin;
    mkdir "$dir/scripts";
        my $cmdfile="$dir/rerun-pro.sh";
        open CMD,'>',$cmdfile;
        print CMD "cd ",&getcwd,"\n";
#        print CMD &escaped_cmdline,"\n"; #buggy - last arg is quoted.
        my $cline=&cmdline."\n";
        print CMD $cline;
        close CMD;
        print STDERR $cline;
        chmod(0755,$cmdfile);
	check_call("cp $initial_weights $dir/weights.0");
	die "Can't find weights.0" unless (-e "$dir/weights.0");
	}
	write_config(*STDERR);
}


# Generate initial files and values
check_call("cp $iniFile $newIniFile");
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
my $kbest = "$dir/kbest";
if ($dont_accum) {
  $kbest = '';
} else {
  check_call("mkdir -p $kbest");
  $kbest = "--kbest_repository $kbest";
}

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
        push @allweights, "-w $dir/weights.$im1";
        `rm -f $dir/hgs/*.gz`;
	my $decoder_cmd = "$decoder -c $iniFile --weights$pass_suffix $weightsFile -O $dir/hgs";
	my $pcmd;
	if ($use_make) {
		$pcmd = "cat $srcFile | $parallelize --use-fork -p $pmem -e $logdir -j $jobs --";
	} else {
		$pcmd = "cat $srcFile | $parallelize -p $pmem -e $logdir -j $jobs --";
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
	my $dec_score = check_output("cat $runFile | $SCORER $refs_comma_sep -m $metric");
	chomp $dec_score;
	print STDERR "DECODER SCORE: $dec_score\n";

	# save space
	check_call("gzip -f $runFile");
	check_call("gzip -f $decoderLog");

	# run optimizer
	print STDERR "RUNNING OPTIMIZER AT ";
	print STDERR unchecked_output("date");
	print STDERR " - GENERATE TRAINING EXEMPLARS\n";
	my $mergeLog="$logdir/prune-merge.log.$iteration";

	my $score = 0;
	my $icc = 0;
	my $inweights="$dir/weights.$im1";
	my $outweights="$dir/weights.$iteration";
	$cmd="$MAPINPUT $dir/hgs > $dir/agenda.$im1";
	print STDERR "COMMAND:\n$cmd\n";
	check_call($cmd);
	$cmd="$MAPPER $refs_comma_sep -m $metric -i $dir/agenda.$im1 $kbest -w $inweights > $outweights";
	check_call($cmd);
	$lastWeightsFile = $outweights;
	$iteration++;
	`rm hgs/*.gz`;
	print STDERR "\n==========\n";
}

print STDERR "\nFINAL WEIGHTS: $lastWeightsFile\n(Use -w <this file> with the decoder)\n\n";

print STDOUT "$lastWeightsFile\n";

exit 0;

sub get_lines {
  my $fn = shift @_;
  open FL, "<$fn" or die "Couldn't read $fn: $!";
  my $lc = 0;
  while(<FL>) { $lc++; }
  return $lc;
}

sub get_comma_sep_refs {
  my ($r,$p) = @_;
  my $o = check_output("echo $p");
  chomp $o;
  my @files = split /\s+/, $o;
  return "-$r " . join(" -$r ", @files);
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
	print $fh "MAX ITERATIONS:   $max_iterations\n";
	print $fh "JOBS:             $jobs\n";
	print $fh "HEAD NODE:        $host\n";
	print $fh "PMEM (DECODING):  $pmem\n";
	print $fh "CLEANUP:          $cleanup\n";
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
	die "Empty dev set!" if ($i == 0);
}

sub print_help {

	my $executable = check_output("basename $0"); chomp $executable;
	print << "Help";

Usage: $executable [options] <ini file>

	$executable [options] <ini file>
		Runs a complete PRO optimization using the ini file specified.

Required:

	--ref-files <files>
		Dev set ref files.  This option takes only a single string argument.
		To use multiple files (including file globbing), this argument should
		be quoted.

	--source-file <file>
		Dev set source file.

	--weights <file>
		Initial weights file (use empty file to start from 0)

General options:

	--help
		Print this message and exit.

	--dont-accumulate
		Don't accumulate k-best lists from multiple iterations.

	--max-iterations <M>
		Maximum number of iterations to run.  If not specified, defaults
		to $default_max_iter.

	--metric <method>
		Metric to optimize.
		Example values: IBM_BLEU, NIST_BLEU, Koehn_BLEU, TER, Combi

	--pass-suffix <S>
		If the decoder is doing multi-pass decoding, the pass suffix "2",
		"3", etc., is used to control what iteration of weights is set.

	--workdir <dir>
		Directory for intermediate and output files.  If not specified, the
		name is derived from the ini filename.  Assuming that the ini
		filename begins with the decoder name and ends with ini, the default
		name of the working directory is inferred from the middle part of
		the filename.  E.g. an ini file named decoder.foo.ini would have
		a default working directory name foo.

Regularization options:

	--reg <F>
		l2 regularization strength [default=500]. The greater this value,
		the closer to zero the weights will be.

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
