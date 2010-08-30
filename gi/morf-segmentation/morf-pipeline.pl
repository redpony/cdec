#!/usr/bin/perl -w
use strict;
use File::Copy;


# Preprocessing pipeline to take care of word segmentation
# Learns a segmentation model for each/either side of the parallel corpus using all train/dev/test data
# Applies the segmentation where necessary.
# Learns word alignments on the preprocessed training data.
# Outputs script files used later to score output.


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
my $SPLIT_TYPE = ""; #possible values: s, t, st, or (empty string)
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

usage() unless scalar @ARGV >= 2;

my %CORPUS; # for (src,trg) it has (orig, name, filtered, final)

$CORPUS{'src'}{'orig'} = $ARGV[0];
open F, "<$CORPUS{'src'}{'orig'}" or die "Can't read $CORPUS{'src'}{'orig'}: $!"; close F;
$CORPUS{'src'}{'name'} = get_basename($CORPUS{'src'}{'orig'});

$CORPUS{'trg'}{'orig'} = $ARGV[1];
open F, "<$CORPUS{'trg'}{'orig'}" or die "Can't read $CORPUS{'trg'}{'orig'}: $!"; close F;
$CORPUS{'trg'}{'name'} = get_basename($CORPUS{'trg'}{'orig'});

my %DEV; # for (src,trg) has (orig, final.split final.unsplit
if (@ARGV >= 4) {
  $DEV{'src'}{'orig'} = $ARGV[2];
  open F, "<$DEV{'src'}{'orig'}" or die "Can't read $DEV{'src'}{'orig'}: $!"; close F;
  $DEV{'src'}{'name'} = get_basename($DEV{'src'}{'orig'});
  $DEV{'trg'}{'orig'} = $ARGV[3];
  open F, "<$DEV{'trg'}{'orig'}" or die "Can't read $DEV{'trg'}{'orig'}: $!"; close F;
  $DEV{'trg'}{'name'} = get_basename($DEV{'trg'}{'orig'});
}

my %TEST; # for (src,trg) has (orig, name) 
if (@ARGV >= 6) {
  $TEST{'src'}{'orig'} = $ARGV[4];
  open F, "<$TEST{'src'}{'orig'}" or die "Can't read $TEST{'src'}{'orig'}: $!"; close F;
  $TEST{'src'}{'name'} = get_basename($TEST{'src'}{'orig'});
  $TEST{'trg'}{'orig'} = $ARGV[5];
  open F, "<$TEST{'trg'}{'orig'}" or die "Can't read $TEST{'trg'}{'orig'}: $!"; close F;
  $TEST{'trg'}{'name'} = get_basename($TEST{'trg'}{'orig'});
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
  apply_segmentation_side("src", $MODEL_SRC_DIR);  
}

#assume that unsplit hypotheses will be scored against an aritificially split target test set; thus obtain a target splitting model  
#TODO: add a flag to override this behaviour
safemkdir($MODEL_TRG_DIR) or die "Couldn't create output directory $MODEL_TRG_DIR: $!";
learn_segmentation("trg");
$TEST{'trg'}{'finalunsplit'} = "$PROCESSED_DIR/$TEST{'trg'}{'name'}";
copy($TEST{'trg'}{'orig'}, $TEST{'trg'}{'finalunsplit'}) or die "Could not copy unsegmented test set";  

if ($SPLIT_TRG) {
  apply_segmentation_side("trg", $MODEL_TRG_DIR);  
  } else {
  $TEST{'trg'}{'finalsplit'} = "$PROCESSED_DIR/$TEST{'trg'}{'name'}.split";
  apply_segmentation_any($MODEL_TRG_DIR, $TEST{'trg'}{'finalunsplit'}, $TEST{'trg'}{'finalsplit'});  
}

write_eval_sh("$PROCESSED_DIR/eval-devtest.sh");

#copy corpora if they haven't been put in place by splitting operations
place_missing_data_side('src');
place_missing_data_side('trg');

do_align();

if ($CORPUS{'src'}{'orig'} && $DEV{'src'}{'orig'} && $TEST{'src'}{'orig'}) {
  print STDERR "Putting the config file entry in $PROCESSED_DIR/exp.config\n";
#format is:
  # nlfr100k_unsplit /export/ws10smt/jan/nlfr/morfwork/s100k.w40.sp_0 corpus.nl-fr.al fr-3.lm.gz dev.nl dev.fr test2008.nl eval-devtest.sh
  my $line = split_name() . " $PROCESSED_DIR corpus.src-trg.al LMFILE.lm.gz";
  $line = $line . " $DEV{'src'}{'name'} $DEV{'trg'}{'name'}";
  $line = $line . " " . get_basename($TEST{'src'}{$SPLIT_SRC ? "finalsplit" : "finalunsplit"}) . " eval-devtest.sh";
  safesystem("echo '$line' > $PROCESSED_DIR/exp.config");
}

system("date");
print STDERR "All done. You now need to train a language model (if target split), put it in the right dir and update the config file.\n\n";

############################## BILINGUAL ###################################

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

  $corpus = $CORPUS{$WHICH}{'filtered'};
  $dev = $DEV{$WHICH}{'orig'};
  $test = $TEST{$WHICH}{'orig'};

  if ($WHICH eq "src") {
    $moddir = $MODEL_SRC_DIR;
    $ppl = $PPL_SRC;
  } else {
    $moddir = $MODEL_TRG_DIR;
    $ppl = $PPL_TRG;
  }
  my $cmd = "cat $corpus";
  if ($dev) { $cmd = "$cmd $dev"; }
  if ($test) { $cmd = "$cmd $test"; }
  my $tmpfile = "$CORPUS_DIR/all.tmp.gz";
  safesystem("$cmd | $GZIP > $tmpfile") or die "Failed to concatenate data for model learning..";
  assert_marker($tmpfile);

  learn_segmentation_side($tmpfile, $moddir, $ppl, $WHICH);
  safesystem("rm $tmpfile");
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
    
  #decorate training files with identifiers to stop the aligner from training on dev and test when rerun in future.
  safesystem("cd $PROCESSED_DIR && ln -s $CORPUS{'src'}{'name'} corpus.src") or die "Failed to symlink: $!";
  safesystem("cd $PROCESSED_DIR && ln -s $CORPUS{'trg'}{'name'} corpus.trg") or die "Failed to symlink: $!";

  write_wconf($conf_file, $PROCESSED_DIR);  
  system("java -d64 -Xmx24g -jar $ALIGNER ++$conf_file > $ALIGNMENT_DIR/aligner.log");

  if (! -f $ALIGNMENTS) { die "Failed to run word alignment.";}

  my $cmd = "paste $PROCESSED_DIR/corpus.src $PROCESSED_DIR/corpus.trg $ALIGNMENTS";
  $cmd = $cmd . " | sed 's/\\t/ \|\|\| /g' > $PROCESSED_DIR/corpus.src-trg.al";
  safesystem($cmd) or die "Failed to paste into aligned corpus file.";

}

############################# MONOLINGUAL #################################

#copy the necessary data files that weren't place by segmentation
sub place_missing_data_side {
  my $side = shift;

  ifne_copy($CORPUS{$side}{'filtered'}, "$PROCESSED_DIR/$CORPUS{$side}{'name'}") ;

  if ($DEV{$side}{'orig'} && ! -f "$PROCESSED_DIR/$DEV{$side}{'name'}") {
    $DEV{$side}{'final'} = "$PROCESSED_DIR/$DEV{$side}{'name'}";
    copy($DEV{$side}{'orig'}, $DEV{$side}{'final'}) or die "Copy failed: $!";
  }

  if ($TEST{$side}{'orig'} && ! -f "$PROCESSED_DIR/$TEST{$side}{'name'}" && ! $TEST{$side}{'finalunsplit'}) {
    $TEST{$side}{'finalunsplit'} = "$PROCESSED_DIR/$TEST{$side}{'name'}";
    copy($TEST{$side}{'orig'}, $TEST{$side}{'finalunsplit'}) or die "Copy failed: $!";
  }

}

sub apply_segmentation_side {
  my ($side, $moddir) = @_;
 
  print STDERR "\n!!!APPLYING SEGMENTATION MODEL ($side)!!!\n";
  apply_segmentation_any($moddir, $CORPUS{$side}{'filtered'}, "$PROCESSED_DIR/$CORPUS{$side}{'name'}");
  if ($DEV{$side}{'orig'}) {
     $DEV{$side}{'final'} = "$PROCESSED_DIR/$DEV{$side}{'name'}";
    apply_segmentation_any($moddir, $DEV{$side}{'orig'}, "$DEV{$side}{'final'}");
  }
  if ($TEST{$side}{'orig'}) {
    $TEST{$side}{'finalsplit'} = "$PROCESSED_DIR/$TEST{$side}{'name'}.split";
    apply_segmentation_any($moddir, $TEST{$side}{'orig'}, $TEST{$side}{'finalsplit'} );
  } 

}

sub learn_segmentation_side {
  my($INPUT_FILE, $SEGOUT_DIR, $PPL, $LANG) = @_;

  print STDERR "\n!!!LEARNING SEGMENTATION MODEL ($LANG)!!!\n";
  system("date");
  my $SEG_FILE = $SEGOUT_DIR . "/segmentation.ready";
   if ( -f $SEG_FILE) {
    print STDERR "$SEG_FILE exists, reusing...\n";
    return;
  }
  my $cmd = "$MORF_TRAIN $INPUT_FILE $SEGOUT_DIR $PPL \"$MARKER\"";
  safesystem($cmd) or die "Failed to learn segmentation model";
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

##################### PATH FUNCTIONS ##########################

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

sub processed_dir {
  return corpus_dir() . "." . split_name();
}

########################## HELPER FUNCTIONS ############################

sub ifne_copy {
  my ($src, $dest) = @_;
  if (! -f $dest) {
    copy($src, $dest) or die "Copy failed: $!";
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

sub usage {
  print <<EOT;

Usage: $0 [OPTIONS] corpus.src corpus.trg [dev.src dev.trg [test.src test.trg]]

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

sub assert_marker {
  my $file = shift;
  my $result = `zcat $file| grep '$MARKER' | wc -l` or die "Cannot read $file: $!";
  print $result; 
  if (scalar($result) != 0) { die "Data contains marker '$MARKER'; use something else.";}
}
########################### Dynamic config files ##############################

sub write_wconf {
  my ($filename, $train_dir) = @_;
  open WCONF, ">$filename" or die "Can't write $filename: $!";

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
iters   5 5

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

EVAL_MAIN=/export/ws10smt/data/eval.sh
marker="$MARKER"
EOT

  if ($SPLIT_TRG) {
    print EVALFILE <<EOT;
echo "OUTPUT EVALUATION"
echo "-----------------"
\$EVAL_MAIN "\$1" $TEST{'trg'}{'finalsplit'}

echo "RECOMBINED OUTPUT EVALUATION"
echo "----------------------------"
cat "\$1" | sed -e "s/\$marker \$marker//g" -e "s/\$marker//g" > "\$1.recombined"

\$EVAL_MAIN "\$1.recombined" $TEST{'trg'}{'finalunsplit'}
EOT

  } else {
    print EVALFILE <<EOT;
echo "ARTIFICIAL SPLIT EVALUATION"
echo "--------------------------"

#split the output translation
cat "\$1" | $MORF_SEGMENT $MODEL_TRG_DIR/inputvocab.gz $MODEL_TRG_DIR/segmentation.ready "\$MARKER" > "\$1.split"

\$EVAL_MAIN "\$1.split" $TEST{'trg'}{'finalsplit'}

echo "DIRECT EVALUATION"
echo "--------------------------"
\$EVAL_MAIN "\$1" $TEST{'trg'}{'finalunsplit'}
  
EOT

  }
  close EVALFILE;

}




