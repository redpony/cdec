#!/usr/bin/perl -w
use strict;
use File::Copy;

#WARNING.. THIS SCRIPT IS CURRENTLY SOMEWHAT BROKEN. AND UGLY.

my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path cwd /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }

use Getopt::Long "GetOptions";

my $GZIP = 'gzip';
my $ZCAT = 'gunzip -c';
my $SED = 'sed -e';

my $MORF_TRAIN = "$SCRIPT_DIR/morftrain.sh";
my $MORF_SEGMENT = "$SCRIPT_DIR/morfsegment.py";

my $LINESTRIPPER = "$SCRIPT_DIR/linestripper.py";
my $ALIGNER = "/export/ws10smt/software/berkeleyaligner/berkeleyaligner.jar";
#java -d64 -Xmx10g -jar $ALIGNER ++word-align.conf >> aligner.log
assert_exec($MORF_TRAIN, $LINESTRIPPER, $MORF_SEGMENT, $ALIGNER);

my $OUTPUT = './morfwork';
my $PPL_SRC = 50;
my $PPL_TRG = 50;
my $MARKER = "#";
my $MAX_WORDS = 40;
my $SENTENCES;# = 100000;
my $SPLIT_TYPE = "";
my $NAME_SHORTCUT;

usage() unless &GetOptions('max_words=i' => \$MAX_WORDS,
                           'output=s' => \$OUTPUT,
                           'ppl_src=i' => \$PPL_SRC,
                           'ppl_trg=i' => \$PPL_TRG,
                           'sentences=i' => \$SENTENCES,
                           'marker=s' => \$MARKER,
                           'split=s' => \$SPLIT_TYPE,
                           'get_name_only' => \$NAME_SHORTCUT,
                          );
#if ($NAME_SHORTCUT) {
#  print STDERR labeled_dir();
#  exit 0;
#}

usage() unless scalar @ARGV >= 2;

my %CORPUS; # for (src,trg) it has (orig, name, filtered, final)

$CORPUS{'src'}{'orig'} = $ARGV[0];
open F, "<$CORPUS{'src'}{'orig'}" or die "Can't read $CORPUS{'src'}{'orig'}: $!"; close F;
$CORPUS{'src'}{'name'} = get_basename($CORPUS{'src'}{'orig'});

$CORPUS{'trg'}{'orig'} = $ARGV[1];
open F, "<$CORPUS{'trg'}{'orig'}" or die "Can't read $CORPUS{'trg'}{'orig'}: $!"; close F;
$CORPUS{'trg'}{'name'} = get_basename($CORPUS{'trg'}{'orig'});


my %DEV; # for (src,trg) has (orig, final.split final.unsplit


#my %TEST_SRC; #original, final
              #trg has original, final.split final.recombined

my $TEST_SRC;
my $TEST_TRG;

my $TEST_SRC_name;
my $TEST_TRG_name;

if (@ARGV >= 4) {
  $DEV{'src'}{'orig'} = $ARGV[2];
  open F, "<$DEV{'src'}{'orig'}" or die "Can't read $DEV{'src'}{'orig'}: $!"; close F;
  $DEV{'src'}{'name'} = get_basename($DEV{'src'}{'orig'});
  $DEV{'trg'}{'orig'} = $ARGV[3];
  open F, "<$DEV{'trg'}{'orig'}" or die "Can't read $DEV{'trg'}{'orig'}: $!"; close F;
  $DEV{'trg'}{'name'} = get_basename($DEV{'trg'}{'orig'});
}
if (@ARGV >= 6) {
  $TEST_SRC = $ARGV[4];
  open F, "<$TEST_SRC" or die "Can't read $TEST_SRC: $!"; close F;
  $TEST_SRC_name = get_basename($TEST_SRC);
  $TEST_TRG = $ARGV[5];
  open F, "<$TEST_TRG" or die "Can't read $TEST_TRG: $!"; close F;
  $TEST_TRG_name = get_basename($TEST_TRG);
}

my $SPLIT_SRC; #use these to check whether that part is being split
my $SPLIT_TRG;

#OUTPUT WILL GO IN THESE
my $CORPUS_DIR = $OUTPUT . '/' . corpus_dir();            #subsampled corpus
my $MODEL_SRC_DIR = $OUTPUT . '/' . model_dir("src"); #splitting..
my $MODEL_TRG_DIR = $OUTPUT . '/' . model_dir("trg"); # .. models
my $PROCESSED_DIR = $OUTPUT . '/' . processed_dir();      #segmented copora+alignments
my $ALIGNMENT_DIR = $PROCESSED_DIR . '/alignments';

$CORPUS{'src'}{'filtered'} = $CORPUS_DIR . "/$CORPUS{'src'}{'name'}";
$CORPUS{'trg'}{'filtered'} = $CORPUS_DIR . "/$CORPUS{'trg'}{'name'}";

print STDERR "Output: $OUTPUT\n";
print STDERR "Corpus: $CORPUS_DIR\n";
print STDERR "Model-src: $MODEL_SRC_DIR\n";
print STDERR "Model-trg: $MODEL_TRG_DIR\n";
print STDERR "Finaldir: $PROCESSED_DIR\n";

safemkdir($OUTPUT) or die "Couldn't create output directory $OUTPUT: $!";
safemkdir($CORPUS_DIR) or die "Couldn't create output directory $CORPUS_DIR: $!";
filter_corpus();

safemkdir($PROCESSED_DIR);
safemkdir($ALIGNMENT_DIR);

if ($SPLIT_SRC) {
  safemkdir($MODEL_SRC_DIR) or die "Couldn't create output directory $MODEL_SRC_DIR: $!";
  learn_segmentation("src");
  apply_segmentation("src");  
}
if ($SPLIT_TRG) {
  safemkdir($MODEL_TRG_DIR) or die "Couldn't create output directory $MODEL_TRG_DIR: $!";
  learn_segmentation("trg");
  apply_segmentation("trg");  
}

#copy corpora if they haven't been put in place by splitting operations
if (! -f "$PROCESSED_DIR/$CORPUS{'src'}{'name'}") {
  copy($CORPUS{'src'}{'filtered'}, "$PROCESSED_DIR/$CORPUS{'src'}{'name'}") or die "Copy failed: $!";
}
if (! -f "$PROCESSED_DIR/$CORPUS{'trg'}{'name'}") {
  copy($CORPUS{'trg'}{'filtered'}, "$PROCESSED_DIR/$CORPUS{'trg'}{'name'}") or die "Copy failed: $!";
}
if ($DEV{'src'}{'orig'} && ! -f "$PROCESSED_DIR/$DEV{'src}{'name'}") {
  copy(
}
if ($TEST_SRC) { ifne_copy($TEST_SRC, "$PROCESSED_DIR/$TEST_SRC_name"); }
if ($TEST_TRG) { ifne_copy("$TEST_TRG.unsplit", "$PROCESSED_DIR/$TEST_TRG_name.unsplit"); }



do_align();

system("date");
print STDERR "All done. You now need to train a language model (if target split), preprocess the test data and put various things where the eval scripts can find them\n\n".

sub ifne_copy {
  my ($src, $dest) = @_;
  if (! -f $dest) {
    copy($src, $dest) or die "Copy failed: $!";
  }
}

}

sub filter_corpus {
  print STDERR "\n!!!FILTERING TRAINING COPRUS!!!\n";
  if ( -f $CORPUS{'src'}{'filtered'} && -f $CORPUS{'trg'}{'filtered'}) {
    print STDERR "$CORPUS{'src'}{'filtered'} and $CORPUS{'trg'}{'filtered'} exist, reusing...\n";
    return;
  }
  my $args = "$CORPUS{'src'}{'orig'} $CORPUS{'trg'}{'orig'} $MAX_WORDS";
  if ($SENTENCES) { $args = $args . " $SENTENCES"; } 
  safesystem("$LINESTRIPPER $args 1> $CORPUS{'src'}{'filtered'} 2> $CORPUS{'trg'}{'filtered'}") or die "Failed to filter training corpus for length.";
}

sub learn_segmentation
{
  my $WHICH = shift;
  my $corpus; my $dev; my $test; my $moddir;  my $ppl;

  if ($WHICH eq "src") {
    print STDERR "\n!!!LEARNING SEGMENTATION MODEL (SOURCE LANGUAGE)!!!\n";
    $corpus = $CORPUS{'src'}{'filtered'};
    $dev = $DEV{'src'}{'orig'};
    $test = $TEST_SRC;
    $moddir = $MODEL_SRC_DIR;
    $ppl = $PPL_SRC;
  } else {
    print STDERR "\n!!!LEARNING SEGMENTATION MODEL (TARGET LANGUAGE)!!!\n";
    $corpus = $CORPUS{'trg'}{'filtered'};
    $dev = $DEV{'trg'}{'orig'};
    $test = $TEST_TRG;
    $moddir = $MODEL_TRG_DIR;
    $ppl = $PPL_TRG;
  }
  system("date");
  my $cmd = "cat $corpus";
  if ($dev) { $cmd = "$cmd $dev"; }
  if ($test) { $cmd = "$cmd $test"; }
  my $tmpfile = "$CORPUS_DIR/all.tmp.gz";
  safesystem("$cmd | $GZIP > $tmpfile") or die "Failed to concatenate data for model learning..";
  learn_segmentation_any($tmpfile, $moddir, $ppl);
  safesystem("rm $tmpfile");
}

sub learn_segmentation_any {
  my($INPUT_FILE, $SEGOUT_DIR, $PPL) = @_;
  my $SEG_FILE = $SEGOUT_DIR . "/segmentation.ready";
   if ( -f $SEG_FILE) {
    print STDERR "$SEG_FILE exists, reusing...\n";
    return;
  }
  my $cmd = "$MORF_TRAIN $INPUT_FILE $SEGOUT_DIR $PPL \"$MARKER\"";
  safesystem($cmd) or die "Failed to learn segmentation model";
}

sub do_align {
  print STDERR "\n!!!WORD ALIGNMENT!!!\n";
  system("date");

  my $ALIGNMENTS = "$ALIGNMENT_DIR/training.align";
  if ( -f $ALIGNMENTS ) {
    print STDERR "$ALIGNMENTS  exists, reusing...\n";
    return;
  } 
  my $conf_file = "$ALIGNMENT_DIR/word-align.conf";
    
  #decorate training files with identifiers to stop the aligner from training on dev and test too
  #since they are in same directory
  safesystem("cd $PROCESSED_DIR && ln -s $CORPUS{'src'}{'name'} corpus.src") or die "Failed to symlink: $!";
  safesystem("cd $PROCESSED_DIR && ln -s $CORPUS{'trg'}{'name'} corpus.trg") or die "Failed to symlink: $!";

  write_wconf($conf_file, $PROCESSED_DIR);  
  safesystem("java -d64 -Xmx24g -jar $ALIGNER ++$conf_file > $ALIGNMENT_DIR/aligner.log") or die "Failed to run word alignment.";

}

sub apply_segmentation {
  my $WHICH = shift;
  my $moddir;
  my $datfile;
  if ($WHICH eq "src") {
    print STDERR "\n!!!APPLYING SEGMENTATION MODEL (SOURCE LANGUAGE)!!!\n";
    apply_segmentation_any($MODEL_SRC_DIR, $CORPUS{'src'}{'filtered'}, "$PROCESSED_DIR/$CORPUS{'src'}{'name'}");
    if ($DEV{'src'}{'orig'}) {
      apply_segmentation_any($MODEL_SRC_DIR, $DEV{'src'}{'orig'}, "$PROCESSED_DIR/$DEV{'src'}{'name'}");
    }
    if ($TEST_SRC) {
      apply_segmentation_any($MODEL_SRC_DIR, $TEST_SRC, "$PROCESSED_DIR/$TEST_SRC_name");
    }
  } else {
    print STDERR "\n!!!APPLYING SEGMENTATION MODEL (TARGET LANGUAGE)!!!\n";
    apply_segmentation_any($MODEL_TRG_DIR, $CORPUS{'trg'}{'filtered'}, "$PROCESSED_DIR/$CORPUS{'trg'}{'name'}");
    if ($DEV{'trg'}{'orig'}) {
      $DEV{'trg'}{'final'} = "$PROCESSED_DIR/$DEV{'trg'}{'name'}";
      apply_segmentation_any($MODEL_TRG_DIR, $DEV{'trg'}{'orig'}, $DEV{'trg'}{'final'});
    }
    if ($TEST_TRG) {
      apply_segmentation_any($MODEL_TRG_DIR, $TEST_TRG, "$PROCESSED_DIR/$TEST_TRG_name.split");
      copy($TEST_TRG, "$PROCESSED_DIR/$TEST_TRG_name.unsplit") or die "Could not copy unsegmented test set";
    }
  }
  if ($WHICH eq "src" || $WHICH eq "trg") {
      write_eval_sh("$PROCESSED_DIR/eval-devtest.sh");
  }
}

sub apply_segmentation_any {
  my($moddir, $datfile, $outfile) = @_;
  if ( -f $outfile) {
    print STDERR "$outfile exists, reusing...\n";
    return;
  }
  
  my $args = "$moddir/inputvocab.gz $moddir/segmentation.ready \"$MARKER\"";
  safesystem("cat $datfile | $MORF_SEGMENT $args &> $outfile") or die "Could not segment $datfile";
}

sub beautify_numlines {
  return ($SENTENCES ? $SENTENCES : "_all");
}

sub corpus_dir {
  return "s" . beautify_numlines() . ".w" . $MAX_WORDS;
}

sub model_dir {
  my $lang = shift;
  if ($lang eq "src") { 
    return corpus_dir() . ".PPL" . $PPL_SRC . ".src";
  } elsif ($lang eq "trg") {
    return corpus_dir() .  ".PPL" . $PPL_TRG . ".trg";
  } else {
    return "PPLundef";
  }    
}

sub split_name {
  #parses SPLIT_TYPE, which can have the following values
  # t|s|ts|st (last 2 are equiv)
  # or is undefined when no splitting is done
  my $name = "";
  
  if ($SPLIT_TYPE) { 
    $SPLIT_SRC = lc($SPLIT_TYPE) =~ /s/;
    $SPLIT_TRG = lc($SPLIT_TYPE) =~ /t/;
    $name = $name . ($SPLIT_SRC ? $PPL_SRC : "0");
    $name = $name . "_" . ($SPLIT_TRG ? $PPL_TRG : "0"); 
  } else {
    #no splitting
    $name = "0";
  }

  return "sp_" . $name;
  
}

sub processed_dir {
  return corpus_dir() . "." . split_name;
}

sub usage {
  print <<EOT;

Usage: $0 [OPTIONS] corpus.src corpus.trg dev.src dev.trg test.src test.trg

Learns a segmentation model and splits up corpora as necessary. Word alignments are trained on a specified subset of the training corpus.

EOT
  exit 1;
};

sub safemkdir {
  my $dir = shift;
  if (-d $dir) { return 1; }
  return mkdir($dir);
}

sub assert_exec {
  my @files = @_;
  for my $file (@files) {
    die "Can't find $file - did you run make?\n" unless -e $file;
    die "Can't execute $file" unless -e $file;
  }
};
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

sub get_basename
{
  my $x = shift;
  $x = `basename $x`;
  $x =~ s/\n//;
  return $x;
}

sub write_wconf {
  my ($filename, $train_dir) = @_;
  open WCONF, ">$filename" or die "Can't write $filename: $!";

#TODO CHANGE ITERATIONS BELOW!!!
  print WCONF <<EOT;
## ----------------------
## This is an example training script for the Berkeley
## word aligner.  In this configuration it uses two HMM
## alignment models trained jointly and then decoded
## using the competitive thresholding heuristic.

##########################################
# Training: Defines the training regimen
##########################################
forwardModels   MODEL1 HMM
reverseModels   MODEL1 HMM
mode    JOINT JOINT
iters   1 1

###############################################
# Execution: Controls output and program flow
###############################################
execDir $ALIGNMENT_DIR
create
overwriteExecDir
saveParams  true
numThreads  1
msPerLine   10000
alignTraining

#################
# Language/Data
#################
foreignSuffix   src
englishSuffix   trg

# Choose the training sources, which can either be directories or files that list files/directories
trainSources    $train_dir/
#trainSources     $train_dir/sources
testSources     
sentences   MAX

#################
# 1-best output
#################
competitiveThresholding

EOT
  close WCONF;
}

sub write_eval_sh
{
  my ($filename) = @_;
  open EVALFILE, ">$filename" or die "Can't write $filename: $!";

  print EVALFILE <<EOT;
#!/bin/bash
d=`dirname \$0`

EVAL_MAIN=/export/ws10smt/data/eval.sh
EOT

  if ($SPLIT_TRG) {
    print EVALFILE <<EOT;
echo "OUTPUT EVALUATION"
echo "-----------------"
\$EVAL_MAIN "\$1" \$d/$TEST_TRG_name.split

echo "RECOMBINED OUTPUT EVALUATION"
echo "----------------------------"
marker="$MARKER"
cat "\$1" | sed -e "s/\$marker \$marker//g" -e "s/\$marker//g" > "\$1.recombined"

\$EVAL_MAIN "\$1.recombined" \$d/$TEST_TRG_name.unsplit
EOT

  } else {
    print EVALFILE <<EOT;
#!/bin/bash
d=`dirname \$0`

EVAL_MAIN=/export/ws10smt/data/eval.sh

echo "ARTIFICIAL SPLIT EVALUATION"
echo "--------------------------"

MARKER="$MARKER"
#split the output translation
cat "\$1" | $MORFSEGMENT $MODEL_TRG_DIR/inputvocab.gz $MODEL_TRG_DIR/segmentation.ready "\$MARKER" > "\$1.split"

\$EVAL_MAIN "i\$1.split" \$d/$TEST_TRG_name.split


echo "DIRECT EVALUATION"
echo "--------------------------"
\$EVAL_MAIN "\$1" \$d/$TEST_TRG_name.unsplit
  
EOT
  }
  close EVALFILE;

}

