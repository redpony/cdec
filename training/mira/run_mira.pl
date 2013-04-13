#!/usr/bin/env perl
use strict;
my @ORIG_ARGV=@ARGV;
use Cwd qw(getcwd);
my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0));
push @INC, $SCRIPT_DIR, "$SCRIPT_DIR/../environment"; }

# Skip local config (used for distributing jobs) if we're running in local-only mode
use LocalConfig;
use Getopt::Long;
use IPC::Open2;
use POSIX ":sys_wait_h";
my $QSUB_CMD = qsub_args(mert_memory());

require "libcall.pl";


my $srcFile;
my $refFiles;
my $bin_dir = $SCRIPT_DIR;
die "Bin directory $bin_dir missing/inaccessible" unless -d $bin_dir;
my $FAST_SCORE="$bin_dir/../mteval/fast_score";
die "Can't execute $FAST_SCORE" unless -x $FAST_SCORE;

my $iteration = 0.0;
my $max_iterations = 6;
my $metric = "ibm_bleu";
my $iniFile;
my $weights;
my $initialWeights;
my $decode_nodes = 1;   # number of decode nodes
my $pmem = "1g";
my $dir;

my $SCORER = $FAST_SCORE;
my $local_server = "$bin_dir/local_parallelize.pl";
my $parallelize = "$bin_dir/../dpmert/parallelize.pl";
my $libcall = "$bin_dir/../dpmert/libcall.pl";
my $sentserver = "$bin_dir/../dpmert/sentserver";
my $sentclient = "$bin_dir/../dpmert/sentclient";
my $run_local_server = 0;
my $run_local = 0;
my $usefork;
my $pass_suffix = '';

my $cdec ="$bin_dir/kbest_mirav5"; #"$bin_dir/kbest_mira_rmmv2"; #"$bin_dir/kbest_mira_lv";

#my $cdec ="$bin_dir/kbest_mira_rmmv2"; #"$bin_dir/kbest_mirav5"; #"$bin_dir/kbest_mira_rmmv2"; #"$bin_dir/kbest_mira_lv";
die "Can't find decoder in $cdec" unless -x $cdec;
my $decoder = $cdec;
my $decoderOpt;
my $update_size=250;
my $approx_score;
my $kbest_size=250;
my $metric_scale=1;
my $optimizer=2;
my $disable_clean = 0;
my $use_make;  # use make to parallelize line search
my $density_prune;
my $cpbin=1;
my $help = 0;
my $epsilon = 0.0001;
my $step_size = 0.01;
my $gpref;
my $unique_kbest;
my $freeze;
my $latent;
my $sample_max;
my $hopes=1;
my $fears=1;

my $range = 35000;
my $minimum = 15000;
my $portn = int(rand($range)) + $minimum;


# Process command-line options
Getopt::Long::Configure("no_auto_abbrev");
if (GetOptions(
        "decoder=s" => \$decoderOpt,
        "decode-nodes=i" => \$decode_nodes,
        "density-prune=f" => \$density_prune,
        "dont-clean" => \$disable_clean,
        "pass-suffix=s" => \$pass_suffix,
        "use-fork" => \$usefork,
        "epsilon=s" => \$epsilon,
        "help" => \$help,
        "local" => \$run_local,
	"local_server" => \$run_local_server,
        "use-make=i" => \$use_make,
        "max-iterations=i" => \$max_iterations,
        "pmem=s" => \$pmem,
        "cpbin!" => \$cpbin,
        "ref-files=s" => \$refFiles,
        "metric=s" => \$metric,
        "source-file=s" => \$srcFile,
        "weights=s" => \$initialWeights,
	"optimizer=i" => \$optimizer,
	"metric-scale=i" => \$metric_scale,
	"kbest-size=i" => \$kbest_size,
	"update-size=i" => \$update_size,
	"step-size=f" => \$step_size,
	"hope-select=i" => \$hopes,
	"fear-select=i" => \$fears,
	"approx-score" => \$approx_score,
	"unique-kbest" => \$unique_kbest,
	"latent" => \$latent,
	"sample-max=i" => \$sample_max,
        "grammar-prefix=s" => \$gpref,
	"freeze" => \$freeze,
        "workdir=s" => \$dir,
	) == 0 || @ARGV!=1 || $help) {
        print_help();
        exit;
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


#my $refs_comma_sep = get_comma_sep_refs($refFiles);
my $refs_comma_sep = get_comma_sep_refs('r',$refFiles);

#my $refs_comma_sep_4cdec = get_comma_sep_refs_4cdec($refFiles);

unless ($dir){
        $dir = "mira";
}
unless ($dir =~ /^\//){  # convert relative path to absolute path
        my $basedir = check_output("pwd");
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




if (-e $dir && dirsize($dir)>1 && -e "$dir/weights" ){ # allow preexisting logfile, binaries, but not dist-vest.pl outputs
    die "ERROR: working dir $dir already exists\n\n";
} else {
    -e $dir || mkdir $dir;
    mkdir "$dir/scripts";
    my $cmdfile="$dir/rerun-mira.sh";
    open CMD,'>',$cmdfile;
    print CMD "cd ",&getcwd,"\n";
    my $cline=&cmdline."\n";
    print CMD $cline;
    close CMD;
    print STDERR $cline;
    chmod(0755,$cmdfile);
    unless (-e $initialWeights) {
        print STDERR "Please specify an initial weights file with --initial-weights\n";
        print_help();
        exit;
    }
    check_call("cp $initialWeights $dir/weights.0");
    die "Can't find weights.0" unless (-e "$dir/weights.0");
}
write_config(*STDERR);

# Generate initial files and values
check_call("cp $iniFile $newIniFile");
$iniFile = $newIniFile;

my $newsrc = "$dir/dev.input";
enseg($srcFile, $newsrc, $gpref);

$srcFile = $newsrc;
my $devSize = 0;
open F, "<$srcFile" or die "Can't read $srcFile: $!";
while(<F>) { $devSize++; }
close F;

my $lastPScore = 0;
my $lastWeightsFile;

# main optimization loop
#while (1){
for (my $opt_iter=0; $opt_iter<$max_iterations; $opt_iter++) {

	print STDERR "\n\nITERATION $opt_iter\n==========\n";
	print STDERR "Using port $portn\n";

	# iteration-specific files
	my $runFile="$dir/run.raw.$opt_iter";
	my $onebestFile="$dir/1best.$opt_iter";
	my $logdir="$dir/logs.$opt_iter";
	my $decoderLog="$logdir/decoder.sentserver.log.$opt_iter";
	my $scorerLog="$logdir/scorer.log.$opt_iter";
	my $weightdir="$dir/weights.pass$opt_iter/";
	check_call("mkdir -p $logdir");
	check_call("mkdir -p $weightdir");

	#decode
	print STDERR "RUNNING DECODER AT ";
	print STDERR unchecked_output("date");
#	my $im1 = $opt_iter - 1;
	my $weightsFile="$dir/weights.$opt_iter";
	print "ITER $iteration " ;
	my $cur_pass = "-p 0$opt_iter";
	my $decoder_cmd = "$decoder -c $iniFile -w $weightsFile $refs_comma_sep -m $metric -s $metric_scale -a -b $update_size -k $kbest_size -o $optimizer $cur_pass -O $weightdir -D $dir  -h $hopes -f $fears -C $step_size";
	if($unique_kbest){
		$decoder_cmd .= " -u";
	}
	if($latent){
		$decoder_cmd .= " -l";
	}
	if($sample_max){
		$decoder_cmd .= " -t $sample_max";
	}
	if ($density_prune) {
		$decoder_cmd .= " --density_prune $density_prune";
	}
	my $pcmd;
	if ($run_local) {
		$pcmd = "cat $srcFile |";
	} elsif ($use_make) {
	    # TODO: Throw error when decode_nodes is specified along with use_make
		$pcmd = "cat $srcFile | $parallelize --use-fork -p $pmem -e $logdir -j $use_make --";
	} elsif ($run_local_server){
	    $pcmd = "cat $srcFile | $local_server $usefork -p $pmem -e $logdir -n $decode_nodes --";
	}
	else {
	    $pcmd = "cat $srcFile | $parallelize $usefork -p $pmem -e $logdir -j $decode_nodes --baseport $portn --";
	}
	my $cmd = "$pcmd $decoder_cmd 2> $decoderLog 1> $runFile";
	print STDERR "COMMAND:\n$cmd\n";
	check_bash_call($cmd);

	my $retries = 0;
        my $num_topbest;
        while($retries < 5) {
            $num_topbest = check_output("wc -l < $runFile");
            print STDERR "NUMBER OF TOP-BEST HYPs: $num_topbest\n";
            if($devSize == $num_topbest) {
                last;
            } else {
                print STDERR "Incorrect number of topbest. Waiting for distributed filesystem and retrying...\n";
                sleep(3);
            }
            $retries++;
        }
	 die "Dev set contains $devSize sentences, but we don't have topbest for all these! Decoder failure? Check $decoderLog\n" if ($devSize != $num_topbest);


	#score the output from this iteration
	open RUN, "<$runFile" or die "Can't read $runFile: $!";
	open H, ">$runFile.H" or die;
	open F, ">$runFile.F" or die;
	open B, ">$runFile.B" or die;
	while(<RUN>) {
	    chomp();
	    (my $hope,my $best,my $fear) = split(/ \|\|\| /);
	    print H "$hope \n"; 	    
	    print B "$best \n";
 	    print F "$fear \n";
	}
	close RUN;
	close F; close B; close H;
	
	my $dec_score = check_output("cat $runFile.B | $SCORER $refs_comma_sep -l $metric");
	my $dec_score_h = check_output("cat $runFile.H | $SCORER $refs_comma_sep -l $metric");
	my $dec_score_f = check_output("cat $runFile.F | $SCORER $refs_comma_sep -l $metric");
	chomp $dec_score; chomp $dec_score_h; chomp $dec_score_f;
	print STDERR "DECODER SCORE: $dec_score HOPE: $dec_score_h FEAR: $dec_score_f\n";

	# save space
	check_call("gzip -f $runFile");
	check_call("gzip -f $decoderLog");
		my $iter_filler="";
	if($opt_iter < 10)
	{$iter_filler="0";}

	my $nextIter = $opt_iter + 1;
	my $newWeightsFile = "$dir/weights.$nextIter";
	$lastWeightsFile = "$dir/weights.$opt_iter";

	average_weights("$weightdir/weights.mira-pass*.*[0-9].gz", $newWeightsFile, $logdir);
#	check_call("cp $lastW $newWeightsFile");
#	if ($icc < 2) {
#		print STDERR "\nREACHED STOPPING CRITERION: score change too little\n";
#		last;
#	}
	system("gzip -f $logdir/kbes*");
	print STDERR "\n==========\n";
	$iteration++;
}
#find 
#my $cmd = `grep SCORE /fs/clip-galep5/lexical_tm/log.runmira.nist.20 | cat -n | sort -k +2 | tail -1`;
#$cmd =~ m/([0-9]+)/;
#$lastWeightsFile = "$dir/weights.$1";
#check_call("ln -s $lastWeightsFile $dir/weights.tuned");
print STDERR "\nFINAL WEIGHTS: $lastWeightsFile\n(Use -w <this file> with the decoder)\n\n";

print STDOUT "$lastWeightsFile\n";

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
        print $fh "GRAMMAR PREFIX:   $gpref\n";
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
    my $grammarpref = shift;

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
	}
	elsif (defined $grammarpref) {
	    print NEWSRC "<seg id=\"$i\" grammar=\"$grammarpref.$i.gz\">$line</seg>\n";}
	else {
	    print NEWSRC "<seg id=\"$i\">$line</seg>\n";
	}
	$i++;
    }
    close SRC;
    close NEWSRC;
}

sub print_help {
	print "Something wrong\n";
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

sub average_weights {

    my $path = shift;
    my $out = shift;
    my $logpath = shift;
    print "AVERAGE $path $out\n";
    my %feature_weights= ();
    my $total =0;
    my $total_mult =0;
    sleep(10);
    foreach my $file (glob "$path")
    {
	$file =~ /\/([^\/]+).gz$/;
	my $fname = $1;
	my $cmd = "gzip -d $file";
	$file =~ s/\.gz//;
	check_bash_call($cmd);
	my $mult = 0;
	print "FILE $file \n";
	open SCORE, "< $file" or next;
	$total++;
	while( <SCORE> ) {
	    my $line = $_;
	    if ($line !~ m/^\#/)
	    {
		my @s = split(" ",$line);
		$feature_weights{$s[0]}+= $mult * $s[1];
	    }
	    else
	    {
		(my $msg,my $ran,$mult) = split(/ \|\|\| /);
		print "RAN $ran $mult\n";
	    }
	}
	$total_mult += $mult;
	
	close SCORE;
	$cmd = "gzip $file"; check_bash_call($cmd);
    }
    
#print out new averaged weights
    open OUT, "> $out" or next;
    for my $f ( keys %feature_weights ) {
	print "$f $feature_weights{$f} $total_mult\n";
	my $ave = $feature_weights{$f} / $total_mult;
	
	print "Printing $f $ave ||| ";
	print OUT "$f $ave\n";
    }
    
}
