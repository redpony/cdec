#!/usr/bin/env perl
use strict;
my @ORIG_ARGV=@ARGV;
use Cwd qw(getcwd);
my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR, "$SCRIPT_DIR/../../environment"; }

# Skip local config (used for distributing jobs) if we're running in local-only mode
use LocalConfig;
use Getopt::Long;
use File::Basename qw(basename);
my $QSUB_CMD = qsub_args(mert_memory());

require "libcall.pl";

# Default settings
my $default_jobs = env_default_jobs();
my $bin_dir = $SCRIPT_DIR;
die "Bin directory $bin_dir missing/inaccessible" unless -d $bin_dir;
my $FAST_SCORE="$bin_dir/../../mteval/fast_score";
die "Can't execute $FAST_SCORE" unless -x $FAST_SCORE;
my $parallelize = "$bin_dir/parallelize.pl";
my $libcall = "$bin_dir/libcall.pl";
my $sentserver = "$bin_dir/sentserver";
my $sentclient = "$bin_dir/sentclient";
my $LocalConfig = "$SCRIPT_DIR/../../environment/LocalConfig.pm";

my $SCORER = $FAST_SCORE;
my $cdec = "$bin_dir/../../decoder/cdec";
die "Can't find decoder in $cdec" unless -x $cdec;
die "Can't find $parallelize" unless -x $parallelize;
die "Can't find $libcall" unless -e $libcall;
my $decoder = $cdec;
my $jobs = $default_jobs;   # number of decode nodes
my $pmem = "9g";
my $help = 0;
my $config;
my $test_set;
my $weights;
my $use_make = 1;
my $useqsub;
my $cpbin=1;
my $base_dir;
# Process command-line options
if (GetOptions(
	"jobs=i" => \$jobs,
	"help" => \$help,
	"qsub" => \$useqsub,
	"input=s" => \$test_set,
        "config=s" => \$config,
	"weights=s" => \$weights,
        "dir=s" => \$base_dir,
) == 0 || @ARGV!=0 || $help) {
	print_help();
	exit;
}

if ($useqsub) {
  $use_make = 0;
  die "LocalEnvironment.pm does not have qsub configuration for this host. Cannot run with --qsub!\n" unless has_qsub();
}

my @missing_args = ();

if (!defined $test_set) { push @missing_args, "--input"; }
if (!defined $config) { push @missing_args, "--config"; }
if (!defined $weights) { push @missing_args, "--weights"; }
die "Please specify missing arguments: " . join (', ', @missing_args) . "\nUse --help for more information.\n" if (@missing_args);

my @tf = localtime(time);
my $tname = basename($test_set);
$tname =~ s/\.(sgm|sgml|xml)$//i;
my $dir = "eval.$tname." . sprintf('%d%02d%02d-%02d%02d%02d', 1900+$tf[5], $tf[4], $tf[3], $tf[2], $tf[1], $tf[0]);
if ($base_dir) {
  $dir = $base_dir.'/'.$dir
}
my $time = unchecked_output("date");

check_call("mkdir -p $dir");

split_devset($test_set, "$dir/test.input.raw", "$dir/test.refs");
my $refs = "-r $dir/test.refs";
my $newsrc = "$dir/test.input";
enseg("$dir/test.input.raw", $newsrc);
my $src_file = $newsrc;
open F, "<$src_file" or die "Can't read $src_file: $!"; close F;

my $test_trans="$dir/test.trans";
my $logdir="$dir/logs";
my $decoderLog="$logdir/decoder.sentserver.log";
check_call("mkdir -p $logdir");

#decode
print STDERR "RUNNING DECODER AT ";
print STDERR unchecked_output("date");
my $decoder_cmd = "$decoder -c $config --weights $weights";
my $pcmd;
if ($use_make) {
	$pcmd = "cat $src_file | $parallelize --workdir $dir --use-fork -p $pmem -e $logdir -j $jobs --";
} else {
	$pcmd = "cat $src_file | $parallelize --workdir $dir -p $pmem -e $logdir -j $jobs --";
}
my $cmd = "$pcmd $decoder_cmd 2> $decoderLog 1> $test_trans";
check_bash_call($cmd);
print STDERR "DECODER COMPLETED AT ";
print STDERR unchecked_output("date");
print STDERR "\nOUTPUT: $test_trans\n\n";
my $bleu = check_output("cat $test_trans | $SCORER $refs -m ibm_bleu");
chomp $bleu;
print STDERR "BLEU: $bleu\n";
print STDOUT "BLEU: $bleu\n";
my $ter = check_output("cat $test_trans | $SCORER $refs -m ter");
chomp $ter;
print STDERR " TER: $ter\n";
open TR, ">$dir/test.scores" or die "Can't write $dir/test.scores: $!";
my $score_report = <<EOT;
### SCORE REPORT #############################################################
        OUTPUT=$test_trans
  SCRIPT INPUT=$test_set
 DECODER INPUT=$src_file
    REFERENCES=$dir/test.refs
------------------------------------------------------------------------------
          BLEU=$bleu
           TER=$ter
##############################################################################
EOT

print TR $score_report;
print STDOUT $score_report;
close TR;
my $sr = unchecked_output("cat $dir/test.scores");
print STDERR "\n\n$sr\n(A copy of this report can be found in $dir/test.scores)\n\n";
exit 0;

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

	$executable --config cdec.ini --weights weights.txt [--jobs N] [--qsub] <testset.in-ref>

Options:

	--help
		Print this message and exit.

	--config <file>
		A path to the cdec.ini file.

	--weights <file>
		A file specifying feature weights.

	--dir <dir>
		Base directory where directory with evaluation results
                will be located.

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

