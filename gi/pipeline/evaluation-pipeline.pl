#!/usr/bin/perl -w
use strict;
use Getopt::Long;
use Cwd;
my $CWD = getcwd;

my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }

my $JOBS = 15;

# featurize_grammar may add multiple features from a single feature extractor
# the key in this map is the extractor name, the value is a list of the extracted features
my $feat_map = {
  "LogRuleCount" => [ "LogRuleCount", "SingletonRule" ] ,
#  "XFeatures" => [ "XFE","XEF" ] ,
  "XFeatures" => [ "XFE","XEF","LabelledEF","LabelledFE"], # ,"XE_Singleton","XF_Singleton"] ,
  "LabelledRuleConditionals" => [ "LabelledFE","LabelledEF" ] ,
  "LexProb" => [ "LexE2F", "LexF2E" ] ,
  "BackoffRule" => [ "BackoffRule" ] ,
  "RulePenalty" => [ "RulePenalty" ] ,
  "LHSProb" => [ "LHSProb" ] ,
  "LabellingShape" => [ "LabellingShape" ] ,
  "GenerativeProb" => [ "GenerativeProb" ] ,
};

my %init_weights = qw(
  EGivenF -0.735245
  FGivenE -0.219391
  Glue -0.306709
  GlueTop 0.0473331
  LanguageModel 2.40403
  LexE2F -0.266989
  LexF2E -0.550373
  LogECount -0.129853
  LogFCount -0.194037
  LogRuleCount 0.256706
  BackoffRule 0.5
  XFE -0.256706
  XEF -0.256706
  XF_Singleton -0.05
  XE_Singleton -0.8
  LabelledFE -0.256706
  LabelledEF -0.256706
  PassThrough -0.9304905
  SingletonE -3.04161
  SingletonF 0.0714027
  SingletonRule -0.889377
  WordPenalty -1.99495
  RulePenalty -0.1
  LabellingShape -0.1
  LHSProb -0.1
  GenerativeProb -0.1
);


# these features are included by default
my @DEFAULT_FEATS = qw( PassThrough Glue GlueTop LanguageModel WordPenalty );



my $CDEC = "$SCRIPT_DIR/../../decoder/cdec";
my $PARALLELIZE = "$SCRIPT_DIR/../../vest/parallelize.pl";
my $EXTOOLS = "$SCRIPT_DIR/../../extools";
die "Can't find extools: $EXTOOLS" unless -e $EXTOOLS && -d $EXTOOLS;
my $VEST = "$SCRIPT_DIR/../../vest";
die "Can't find vest: $VEST" unless -e $VEST && -d $VEST;
my $DISTVEST = "$VEST/dist-vest.pl";
my $FILTER = "$EXTOOLS/filter_grammar";
my $FEATURIZE = "$EXTOOLS/featurize_grammar";
assert_exec($CDEC, $PARALLELIZE, $FILTER, $FEATURIZE, $DISTVEST);

my $numtopics = 25;

my $config = "$SCRIPT_DIR/clsp.config";
if ((scalar @ARGV) >= 2 && ($ARGV[0] eq '-c')) {
  $config = $ARGV[1];
  shift @ARGV; shift @ARGV;
  unless (-f $config) {
    $config = "$SCRIPT_DIR/$config";
    unless (-f $config) {
      $config .= ".config";
    }
  }
}
print STDERR "CORPORA CONFIGURATION: $config\n";
open CONF, "<$config" or die "Can't read $config: $!";
my %paths;
my %corpora;
my %lms;
my %devs;
my %devrefs;
my %tests;
my %testevals;
my $datadir;
print STDERR "       LANGUAGE PAIRS:";
while(<CONF>) {
  chomp;
  next if /^#/;
  next if /^\s*$/;
  s/^\s+//;
  s/\s+$//;
  if (! defined $datadir) { $datadir = $_; next; }
  my ($name, $path, $corpus, $lm, $dev, $devref, @xtests) = split /\s+/;
  $paths{$name} = $path;
  $corpora{$name} = $corpus;
  $lms{$name} = $lm;
  $devs{$name} = $dev;
  $devrefs{$name} = $devref;
  $tests{$name} = $xtests[0];
  $testevals{$name} = $xtests[1];
  print STDERR " $name";
}
print STDERR "\n";

my %langpairs = map { $_ => 1 } qw( btec zhen fbis aren uren nlfr );

my $outdir = "$CWD/exp";
my $help;
my $FEATURIZER_OPTS = '';
my $dataDir = '/export/ws10smt/data';
my @features;
my $bkoffgram;
my $gluegram;
my $usefork;
if (GetOptions(
        "backoff-grammar=s" => \$bkoffgram,
        "glue-grammar=s" => \$gluegram,
        "data=s" => \$dataDir,
        "features=s@" => \@features,
        "use-fork" => \$usefork,
        "jobs=i" => \$JOBS,
        "out-dir=s" => \$outdir,
) == 0 || @ARGV!=2 || $help) {
        print_help();
        exit;
}
if ($usefork) { $usefork="--use-fork"; } else { $usefork = ''; }
my @fkeys = keys %$feat_map;
die "You must specify one or more features with -f. Known features: @fkeys\n" unless scalar @features > 0;
my @xfeats;
for my $feat (@features) {
  my $rs = $feat_map->{$feat};
  if (!defined $rs) { die "DON'T KNOW ABOUT FEATURE $feat\n"; }
  my @xfs = @$rs;
  @xfeats = (@xfeats, @xfs);
  $FEATURIZER_OPTS .= " -f $feat" unless $feat=="BackoffRule";
}
print STDERR "X-FEATS: @xfeats\n";

my $lp = $ARGV[0];
my $grammar = $ARGV[1];
print STDERR "   CORPUS REPO: $dataDir\n";
print STDERR " LANGUAGE PAIR: $lp\n";
die "I don't know about that language pair\n" unless $paths{$lp};
my $corpdir = "$dataDir";
if ($paths{$lp} =~ /^\//) { $corpdir = $paths{$lp}; } else { $corpdir .= '/' . $paths{$lp}; }
die "I can't find the corpora directory: $corpdir" unless -d $corpdir;
print STDERR "       GRAMMAR: $grammar\n";
my $LANG_MODEL = mydircat($corpdir, $lms{$lp});
print STDERR "            LM: $LANG_MODEL\n";
my $CORPUS = mydircat($corpdir, $corpora{$lp});
die "Can't find corpus: $CORPUS" unless -f $CORPUS;

my $dev = mydircat($corpdir, $devs{$lp});
my $drefs = $devrefs{$lp};
die "Can't find dev: $dev\n" unless -f $dev;
die "Dev refs not set" unless $drefs;
$drefs = mydircat($corpdir, $drefs);

my $test = mydircat($corpdir, $tests{$lp});
my $teval = mydircat($corpdir, $testevals{$lp});
#die "Can't find test: $test\n" unless -f $test;
#assert_exec($teval);

`mkdir -p $outdir`;

# CREATE INIT WEIGHTS
print STDERR "\nCREATING INITIAL WEIGHTS FILE: weights.init\n";
my $weights = mydircat($outdir, "weights.init");
write_random_weights_file($weights, @xfeats);

my $bkoff_grmr;
my $glue_grmr;
if($bkoffgram) {
    print STDERR "Placing backoff grammar…\n";
    $bkoff_grmr = mydircat($outdir, "backoff.scfg.gz");
    print STDERR "cp $bkoffgram $bkoff_grmr\n";
    safesystem(undef,"cp $bkoffgram $bkoff_grmr");
}
if($gluegram) {
    print STDERR "Placing glue grammar…\n";
    $glue_grmr = mydircat($outdir, "glue.bo.scfg.gz");
    print STDERR "cp $gluegram $glue_grmr\n";
    safesystem(undef,"cp $gluegram $glue_grmr");
}

# MAKE DEV
print STDERR "\nFILTERING FOR dev...\n";
print STDERR "DEV: $dev (REFS=$drefs)\n";
my $devgrammar = filter($grammar, $dev, 'dev', $outdir);
my $devini = mydircat($outdir, "cdec-dev.ini");
write_cdec_ini($devini, $devgrammar);


# MAKE TEST
print STDERR "\nFILTERING FOR test...\n";
print STDERR "TEST: $test (EVAL=$teval)\n";
`mkdir -p $outdir`;
my $testgrammar = filter($grammar, $test, 'test', $outdir);
my $testini = mydircat($outdir, "cdec-test.ini");
write_cdec_ini($testini, $testgrammar);



# VEST
print STDERR "\nMINIMUM ERROR TRAINING\n";
my $tuned_weights = mydircat($outdir, 'weights.tuned');
if (-f $tuned_weights) {
  print STDERR "TUNED WEIGHTS $tuned_weights EXISTS: REUSING\n";
} else {
  my $cmd = "$DISTVEST $usefork --decode-nodes $JOBS --ref-files=$drefs --source-file=$dev --weights $weights $devini";
  print STDERR "MERT COMMAND: $cmd\n";
  `rm -rf $outdir/vest 2> /dev/null`;
  chdir $outdir or die "Can't chdir to $outdir: $!";
  $weights = `$cmd`;
  die "MERT reported non-zero exit code" unless $? == 0;
  chomp $weights;
  safesystem($tuned_weights, "cp $weights $tuned_weights");
  print STDERR "TUNED WEIGHTS: $tuned_weights\n";
  die "$tuned_weights is missing!" unless -f $tuned_weights;
}

# DECODE
print STDERR "\nDECODE TEST SET\n";
my $decolog = mydircat($outdir, "test-decode.log");
my $testtrans = mydircat($outdir, "test.trans");
my $cmd = "cat $test | $PARALLELIZE $usefork -j $JOBS -e $decolog -- $CDEC -c $testini -w $tuned_weights > $testtrans";
safesystem($testtrans, $cmd) or die "Failed to decode test set!";


# EVALUATE
print STDERR "\nEVALUATE TEST SET\n";
print STDERR "TEST: $testtrans\n";
$cmd = "$teval $testtrans";
safesystem(undef, $cmd) or die "Failed to evaluate!";
exit 0;


sub write_random_weights_file {
  my ($file, @extras) = @_;
  open F, ">$file" or die "Can't write $file: $!";
  my @feats = (@DEFAULT_FEATS, @extras);
  for my $feat (@feats) {
    my $r = rand(1.6);
    my $w = $init_weights{$feat} * $r;
    if ($w == 0) { $w = 0.0001; print STDERR "WARNING: $feat had no initial weight!\n"; }
    print F "$feat $w\n";
  }
  close F;
}

sub filter {
  my ($grammar, $set, $name, $outdir) = @_;
  my $out1 = mydircat($outdir, "$name.filt.gz");
  my $outgrammar = mydircat($outdir, "$name.scfg.gz");
  if (-f $outgrammar) { print STDERR "$outgrammar exists - REUSING!\n"; } else {
    my $cmd = "gunzip -c $grammar | $FILTER -t $set | gzip > $out1";
    safesystem($out1, $cmd) or die "Filtering failed.";
    $cmd = "gunzip -c $out1 | $FEATURIZE $FEATURIZER_OPTS -g $out1 -c $CORPUS | gzip > $outgrammar";
    safesystem($outgrammar, $cmd) or die "Featurizing failed";
  }
  return $outgrammar;
}  

sub mydircat {
 my ($base, $suffix) = @_;
 if ($suffix =~ /^\//) { return $suffix; }
 my $res = $base . '/' . $suffix;
 $res =~ s/\/\//\//g;
 return $res;
}

sub write_cdec_ini {
  my ($filename, $grammar_path) = (@_);
  open CDECINI, ">$filename" or die "Can't write $filename: $!";
  my $glue = ($gluegram ? "$glue_grmr" : "$datadir/glue/glue.scfg.gz");
  print CDECINI <<EOT;
formalism=scfg
cubepruning_pop_limit=100
add_pass_through_rules=true
scfg_extra_glue_grammar=$glue
grammar=$datadir/oov.scfg.gz
grammar=$grammar_path
scfg_default_nt=OOV
scfg_no_hiero_glue_grammar=true
feature_function=WordPenalty
feature_function=LanguageModel -o 3 $LANG_MODEL
EOT
  print CDECINI "grammar=$bkoff_grmr\n" if $bkoffgram;
  close CDECINI;
};

sub print_help {
  print STDERR<<EOT;

Usage: $0 [-c data-config-file] language-pair grammar.bidir.gz [OPTIONS]

Given an induced grammar for an entire corpus (i.e., generated by
local-gi-pipeline.pl), filter and featurize it for a dev and test set,
run MERT, report scores.

EOT
}

sub safesystem {
  my $output = shift @_;
  print STDERR "Executing: @_\n";
  system(@_);
  if ($? == -1) {
      print STDERR "ERROR: Failed to execute: @_\n  $!\n";
      if (defined $output && -e $output) { printf STDERR "Removing $output\n"; `rm -rf $output`; }
      exit(1);
  }
  elsif ($? & 127) {
      printf STDERR "ERROR: Execution of: @_\n  died with signal %d, %s coredump\n",
          ($? & 127),  ($? & 128) ? 'with' : 'without';
      if (defined $output && -e $output) { printf STDERR "Removing $output\n"; `rm -rf $output`; }
      exit(1);
  }
  else {
    my $exitcode = $? >> 8;
    if ($exitcode) {
      print STDERR "Exit code: $exitcode\n";
      if (defined $output && -e $output) { printf STDERR "Removing $output\n"; `rm -rf $output`; }
    }
    return ! $exitcode;
  }
}

sub assert_exec {
  my @files = @_;
  for my $file (@files) {
    die "Can't find $file - did you run make?\n" unless -e $file;
    die "Can't execute $file" unless -e $file;
  }
};

