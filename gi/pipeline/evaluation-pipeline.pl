#!/usr/bin/perl -w
use strict;
use Getopt::Long;
use Cwd;
my $CWD = getcwd;

my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }

my @DEFAULT_FEATS = qw(
  LogRuleCount SingletonRule LexE2F LexF2E WordPenalty
  LanguageModel Glue GlueTop PassThrough);

my %init_weights = qw(
  LogRuleCount 0.2
  SingletonRule -0.6
  LexE2F -0.3
  LexF2E -0.3
  WordPenalty -1.5
  LanguageModel 1.2
  Glue -1.0
  GlueTop 0.00001
  PassThrough -10.0
  X_EGivenF -0.3
  X_FGivenE -0.3
);

my $CDEC = "$SCRIPT_DIR/../../decoder/cdec";
my $PARALLELIZE = "$SCRIPT_DIR/../../vest/parallelize.pl";
my $EXTOOLS = "$SCRIPT_DIR/../../extools";
die "Can't find extools: $EXTOOLS" unless -e $EXTOOLS && -d $EXTOOLS;
my $VEST = "$SCRIPT_DIR/../../vest";
die "Can't find vest: $VEST" unless -e $VEST && -d $VEST;
my $DISTVEST = "$VEST/dist-vest.pl";
my $FILTSCORE = "$EXTOOLS/filter_score_grammar";
assert_exec($CDEC, $PARALLELIZE, $FILTSCORE, $DISTVEST);

my $config = "$SCRIPT_DIR/config.eval";
open CONF, "<$config" or die "Can't read $config: $!";
my %paths;
my %corpora;
my %lms;
my %devs;
my %devrefs;
my %tests;
my %testevals;
print STDERR "LANGUAGE PAIRS:";
while(<CONF>) {
  chomp;
  next if /^#/;
  next if /^\s*$/;
  s/^\s+//;
  s/\s+$//;
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
my $XFEATS;
my $dataDir = '/export/ws10smt/data';
if (GetOptions(
        "xfeats" => \$XFEATS,
        "data=s" => \$dataDir,
) == 0 || @ARGV!=2 || $help) {
        print_help();
        exit;
}
if ($XFEATS) { die "TODO: implement adding of X-features\n"; }

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
die "Can't find test: $test\n" unless -f $test;
assert_exec($teval);

# MAKE DEV
print STDERR "\nFILTERING FOR dev...\n";
print STDERR "DEV: $dev (REFS=$drefs)\n";
`mkdir -p $outdir`;
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


# CREATE INIT WEIGHTS
print STDERR "\nCREATING INITIAL WEIGHTS FILE: weights.init\n";
my $weights = mydircat($outdir, "weights.init");
write_random_weights_file($weights);


# VEST
print STDERR "\nMINIMUM ERROR TRAINING\n";
my $cmd = "$DISTVEST --ref-files=$drefs --source-file=$dev --weights $weights $devini";
print STDERR "MERT COMMAND: $cmd\n";
`rm -rf $outdir/vest 2> /dev/null`;
chdir $outdir or die "Can't chdir to $outdir: $!";
$weights = `$cmd`;
die "MERT reported non-zero exit code" unless $? == 0;
my $tuned_weights = mydircat($outdir, 'weights.tuned');
`cp $weights $tuned_weights`;
print STDERR "TUNED WEIGHTS: $tuned_weights\n";
die "$tuned_weights is missing!" unless -f $tuned_weights;


# DECODE
print STDERR "\nDECODE TEST SET\n";
my $decolog = mydircat($outdir, "test-decode.log");
my $testtrans = mydircat($outdir, "test.trans");
$cmd = "cat $test | $PARALLELIZE -j 20 -e $decolog -- $CDEC -c $testini -w $tuned_weights > $testtrans";
safesystem($cmd) or die "Failed to decode test set!";


# EVALUATE
print STDERR "\nEVALUATE TEST SET\n";
print STDERR "TEST: $testtrans\n";
$cmd = "$teval $testtrans";
safesystem($cmd) or die "Failed to evaluate!";
exit 0;


sub write_random_weights_file {
  my ($file, @extras) = @_;
  open F, ">$file" or die "Can't write $file: $!";
  my @feats = (@DEFAULT_FEATS, @extras);
  if ($XFEATS) { push @feats, "X_FGivenE"; push @feats, "X_EGivenF"; }
  for my $feat (@feats) {
    my $r = rand(1.6);
    my $w = $init_weights{$feat} * $r;
    print F "$feat $w\n";
  }
  close F;
}

sub filter {
  my ($grammar, $set, $name, $outdir) = @_;
  my $outgrammar = mydircat($outdir, "$name.scfg.gz");
  if (-f $outgrammar) { print STDERR "$outgrammar exists - REUSING!\n"; } else {
    my $cmd = "gunzip -c $grammar | $FILTSCORE -c $CORPUS -t $dev | gzip > $outgrammar";
    safesystem($cmd) or die "Can't filter and score grammar!";
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
  print CDECINI <<EOT;
formalism=scfg
cubepruning_pop_limit=100
add_pass_through_rules=true
scfg_extra_glue_grammar=/export/ws10smt/data/glue/glue.scfg.gz
grammar=$grammar_path
feature_function=WordPenalty
feature_function=LanguageModel -o 3 $LANG_MODEL
EOT
  close CDECINI;
};

sub print_help {
  print STDERR<<EOT;

Usage: $0 [OPTIONS] language-pair grammar.bidir.gz

Given an induced grammar for an entire corpus (i.e., generated by
local-gi-pipeline.pl), filter and featurize it for a dev and test set,
run MERT, report scores.

EOT
}

sub safesystem {
  print STDERR "Executing: @_\n";
  system(@_);
  if ($? == -1) {
      print STDERR "ERROR: Failed to execute: @_\n  $!\n";
      exit(1);
  }
  elsif ($? & 127) {
      printf STDERR "ERROR: Execution of: @_\n  died with signal %d, %s coredump\n",
          ($? & 127),  ($? & 128) ? 'with' : 'without';
      exit(1);
  }
  else {
    my $exitcode = $? >> 8;
    print STDERR "Exit code: $exitcode\n" if $exitcode;
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

