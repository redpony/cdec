#!/usr/bin/perl -w
use strict;
use File::Copy;

my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path cwd /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }

use Getopt::Long "GetOptions";

my $GZIP = 'gzip';
my $ZCAT = 'gunzip -c';
my $SED = 'sed -e';
my $BASE_PHRASE_MAX_SIZE = 10;
my $COMPLETE_CACHE = 1;
my $ITEMS_IN_MEMORY = 10000000;  # cache size in extractors
my $NUM_TOPICS = 50;
my $NUM_TOPICS_COARSE = 10;
my $NUM_TOPICS_FINE = $NUM_TOPICS;
my $NUM_SAMPLES = 1000;
my $CONTEXT_SIZE = 1;
my $BIDIR = 0;
my $TOPICS_CONFIG = "pyp-topics.conf";
my $LANGUAGE = "target";
my $LABEL_THRESHOLD = 0;

my $MODEL = "pyp";
my $NUM_EM_ITERS = 100;
my $NUM_PR_ITERS = 0;
my $PR_SCALE_P = 10;
my $PR_SCALE_C = 0;
my $PR_THREADS = 0;

my $EXTOOLS = "$SCRIPT_DIR/../../extools";
die "Can't find extools: $EXTOOLS" unless -e $EXTOOLS && -d $EXTOOLS;
my $PYPTOOLS = "$SCRIPT_DIR/../pyp-topics/src";
die "Can't find pyp-topics: $PYPTOOLS" unless -e $PYPTOOLS && -d $PYPTOOLS;
my $PYPSCRIPTS = "$SCRIPT_DIR/../pyp-topics/scripts";
die "Can't find pyp-topics: $PYPSCRIPTS" unless -e $PYPSCRIPTS && -d $PYPSCRIPTS;
my $PRTOOLS = "$SCRIPT_DIR/../posterior-regularisation";
die "Can't find posterior-regularisation: $PRTOOLS" unless -e $PRTOOLS && -d $PRTOOLS;
my $REDUCER = "$EXTOOLS/mr_stripe_rule_reduce";
my $C2D = "$PYPSCRIPTS/contexts2documents.py";
my $S2L = "$PYPSCRIPTS/spans2labels.py";

my $PYP_TOPICS_TRAIN="$PYPTOOLS/pyp-contexts-train";
my $PREM_TRAIN="$PRTOOLS/prjava/train-PR-cluster.sh";

my $SORT_KEYS = "$SCRIPT_DIR/scripts/sort-by-key.sh";
my $PATCH_CORPUS = "$SCRIPT_DIR/scripts/patch-corpus.pl";
my $EXTRACTOR = "$EXTOOLS/extractor";
my $TOPIC_TRAIN = "$PYPTOOLS/pyp-contexts-train";

assert_exec($PATCH_CORPUS, $SORT_KEYS, $REDUCER, $EXTRACTOR, $PYP_TOPICS_TRAIN, $S2L, $C2D, $TOPIC_TRAIN);

my $BACKOFF_GRAMMAR;
my $DEFAULT_CAT;
my $HIER_CAT;
my $TAGGED_CORPUS;

my $NAME_SHORTCUT;

my $OUTPUT = './giwork';
usage() unless &GetOptions('base_phrase_max_size=i' => \$BASE_PHRASE_MAX_SIZE,
                           'backoff_grammar' => \$BACKOFF_GRAMMAR,
                           'hier_cat' => \$HIER_CAT,
                           'output=s' => \$OUTPUT,
                           'model=s' => \$MODEL,
                           'topics=i' => \$NUM_TOPICS_FINE,
                           'coarse_topics=i' => \$NUM_TOPICS_COARSE,
                           'trg_context=i' => \$CONTEXT_SIZE,
                           'samples=i' => \$NUM_SAMPLES,
                           'label_threshold=f' => \$LABEL_THRESHOLD,
                           'use_default_cat' => \$DEFAULT_CAT,
                           'topics-config=s' => \$TOPICS_CONFIG,
                           'em-iterations=i' => \$NUM_EM_ITERS,
                           'pr-iterations=i' => \$NUM_PR_ITERS,
                           'pr-scale-phrase=f' => \$PR_SCALE_P,
                           'pr-scale-context=f' => \$PR_SCALE_C,
                           'pr-threads=i' => \$PR_THREADS,
                           'tagged_corpus=s' => \$TAGGED_CORPUS,
                           'language=s' => \$LANGUAGE,
                           'get_name_only' => \$NAME_SHORTCUT,
                          );
if ($NAME_SHORTCUT) {
  $NUM_TOPICS = $NUM_TOPICS_FINE;
  print STDERR labeled_dir();
  exit 0;
}
usage() unless scalar @ARGV == 1;
my $CORPUS = $ARGV[0];
open F, "<$CORPUS" or die "Can't read $CORPUS: $!"; close F;

$NUM_TOPICS = $NUM_TOPICS_FINE;

print STDERR "   Output: $OUTPUT\n";
my $DATA_DIR = $OUTPUT . '/corpora';
my $LEX_NAME = 'corpus.f_e_a.lex';
my $CORPUS_LEX = $DATA_DIR . '/' . $LEX_NAME;  # corpus used to extract rules
my $CORPUS_CLUSTER = $DATA_DIR . '/corpus.f_e_a.cluster'; # corpus used for clustering (often identical)

my $CONTEXT_DIR = $OUTPUT . '/' . context_dir();
my $CLUSTER_DIR = $OUTPUT . '/' . cluster_dir();
my $LABELED_DIR = $OUTPUT . '/' . labeled_dir();
my $CLUSTER_DIR_C;
my $CLUSTER_DIR_F;
if($HIER_CAT) {
    $CLUSTER_DIR_F = $CLUSTER_DIR;
    $NUM_TOPICS = $NUM_TOPICS_COARSE;
    $CLUSTER_DIR_C = $OUTPUT . '/' . cluster_dir();
    $NUM_TOPICS = $NUM_TOPICS_FINE;
}
my $GRAMMAR_DIR = $OUTPUT . '/' . grammar_dir();
print STDERR "  Context: $CONTEXT_DIR\n  Cluster: $CLUSTER_DIR\n  Labeled: $LABELED_DIR\n  Grammar: $GRAMMAR_DIR\n";
safemkdir($OUTPUT) or die "Couldn't create output directory $OUTPUT: $!";
safemkdir($DATA_DIR) or die "Couldn't create output directory $DATA_DIR: $!";
safemkdir($CONTEXT_DIR) or die "Couldn't create output directory $CONTEXT_DIR: $!";
safemkdir($CLUSTER_DIR) or die "Couldn't create output directory $CLUSTER_DIR: $!";
if($HIER_CAT) {
    safemkdir($CLUSTER_DIR_C) or die "Couldn't create output directory $CLUSTER_DIR: $!";
}
safemkdir($LABELED_DIR) or die "Couldn't create output directory $LABELED_DIR: $!";
safemkdir($GRAMMAR_DIR) or die "Couldn't create output directory $GRAMMAR_DIR: $!";
if(-e $TOPICS_CONFIG) {
    copy($TOPICS_CONFIG, $CLUSTER_DIR) or die "Copy failed: $!";
}

setup_data();

extract_context();
if (lc($MODEL) eq "pyp") {
    if($HIER_CAT) {
        $NUM_TOPICS = $NUM_TOPICS_COARSE;
        $CLUSTER_DIR = $CLUSTER_DIR_C;
        topic_train();
        $NUM_TOPICS = $NUM_TOPICS_FINE;
        $CLUSTER_DIR = $CLUSTER_DIR_F;
        topic_train();
    } else {
        topic_train();
    }
} elsif (lc($MODEL) eq "prem") {
    prem_train();
} else { die "Unsupported model type: $MODEL. Must be one of PYP or PREM.\n"; }
if($HIER_CAT) {
    $NUM_TOPICS = $NUM_TOPICS_COARSE;
    $CLUSTER_DIR = $CLUSTER_DIR_C;
    label_spans_with_topics();
    $NUM_TOPICS = $NUM_TOPICS_FINE;
    $CLUSTER_DIR = $CLUSTER_DIR_F;
    label_spans_with_topics();
    combine_labelled_spans();
} else {
    label_spans_with_topics();
}
my $res;
if ($BIDIR) {
  $res = grammar_extract_bidir();
} else {
  $res = grammar_extract();
}
print STDERR "\n!!!COMPLETE!!!\n";
print STDERR "GRAMMAR: $res\nYou should probably run: $SCRIPT_DIR/evaluation-pipeline.pl LANGPAIR giwork/ct1s0.L10.PYP.t4.s20.grammar/grammar.gz -f FEAT1 -f FEAT2\n\n";
exit 0;

sub setup_data {
  print STDERR "\n!!!PREPARE CORPORA!!!\n";
  if (-f $CORPUS_LEX && $CORPUS_CLUSTER) {
    print STDERR "$CORPUS_LEX and $CORPUS_CLUSTER exist, reusing...\n";
    return;
  }
  copy($CORPUS, $CORPUS_LEX);
  if ($TAGGED_CORPUS) {
    die "Can't find $TAGGED_CORPUS" unless -f $TAGGED_CORPUS;
    my $cmd="$PATCH_CORPUS $TAGGED_CORPUS $CORPUS_LEX > $CORPUS_CLUSTER";
    safesystem($cmd) or die "Failed to extract contexts.";
  } else {
    symlink($LEX_NAME, $CORPUS_CLUSTER);
  }
}

sub context_dir {
  return "ct${CONTEXT_SIZE}s0.L$BASE_PHRASE_MAX_SIZE.l$LANGUAGE";
}

sub cluster_dir {
    if (lc($MODEL) eq "pyp") {
        return context_dir() . ".PYP.t$NUM_TOPICS.s$NUM_SAMPLES";
    } elsif (lc($MODEL) eq "prem") {
        if ($NUM_PR_ITERS == 0) {
            return context_dir() . ".PREM.t$NUM_TOPICS.ie$NUM_EM_ITERS.ip$NUM_PR_ITERS";
        } else {
            return context_dir() . ".PREM.t$NUM_TOPICS.ie$NUM_EM_ITERS.ip$NUM_PR_ITERS.sp$PR_SCALE_P.sc$PR_SCALE_C";
        }
    }
}

sub labeled_dir {
  if (lc($MODEL) eq "pyp" && $LABEL_THRESHOLD != 0) {
    return cluster_dir() . "_lt$LABEL_THRESHOLD";
  } else {
    return cluster_dir();
  }
}

sub grammar_dir {
  # TODO add grammar config options -- adjacent NTs, etc
  if($HIER_CAT) {
    return cluster_dir() . ".hier$NUM_TOPICS_COARSE-$NUM_TOPICS_FINE.grammar";
  } else {
    return labeled_dir() . ".grammar";
  }
}



sub safemkdir {
  my $dir = shift;
  if (-d $dir) { return 1; }
  return mkdir($dir);
}

sub usage {
  print <<EOT;

Usage: $0 [OPTIONS] corpus.fr-en-al

Induces a grammar using Pitman-Yor topic modeling or Posterior Regularisation.

EOT
  exit 1;
};

sub assert_exec {
  my @files = @_;
  for my $file (@files) {
    die "Can't find $file - did you run make?\n" unless -e $file;
    die "Can't execute $file" unless -e $file;
  }
};

sub extract_context {
 print STDERR "\n!!!CONTEXT EXTRACTION\n"; 
 my $OUT_CONTEXTS = "$CONTEXT_DIR/context.txt.gz";
 if (-e $OUT_CONTEXTS) {
   print STDERR "$OUT_CONTEXTS exists, reusing...\n";
 } else {
   my $cmd = "$EXTRACTOR -i $CORPUS_CLUSTER -c $ITEMS_IN_MEMORY -L $BASE_PHRASE_MAX_SIZE -C -S $CONTEXT_SIZE --phrase_language $LANGUAGE --context_language $LANGUAGE | $SORT_KEYS | $REDUCER | $GZIP > $OUT_CONTEXTS";
   if ($COMPLETE_CACHE) {
     print STDERR "COMPLETE_CACHE is set: removing memory limits on cache.\n";
     $cmd = "$EXTRACTOR -i $CORPUS_CLUSTER -c 0 -L $BASE_PHRASE_MAX_SIZE -C -S $CONTEXT_SIZE  --phrase_language $LANGUAGE --context_language $LANGUAGE  | $SORT_KEYS | $GZIP > $OUT_CONTEXTS";
   }
   safesystem($cmd) or die "Failed to extract contexts.";
  }
}

sub topic_train {
  print STDERR "\n!!!TRAIN PYP TOPICS\n";
  my $IN_CONTEXTS = "$CONTEXT_DIR/context.txt.gz";
  my $OUT_CLUSTERS = "$CLUSTER_DIR/docs.txt.gz";
  if (-e $OUT_CLUSTERS) {
    print STDERR "$OUT_CLUSTERS exists, reusing...\n";
  } else {
    safesystem("$TOPIC_TRAIN --data $IN_CONTEXTS --backoff-type simple -t $NUM_TOPICS -s $NUM_SAMPLES -o $OUT_CLUSTERS -c $TOPICS_CONFIG -w /dev/null") or die "Topic training failed.\n";
  }
}

sub prem_train {
  print STDERR "\n!!!TRAIN PR/EM model\n";
  my $IN_CONTEXTS = "$CONTEXT_DIR/context.txt.gz";
  my $OUT_CLUSTERS = "$CLUSTER_DIR/docs.txt.gz";
  if (-e $OUT_CLUSTERS) {
    print STDERR "$OUT_CLUSTERS exists, reusing...\n";
  } else {
    safesystem("$PREM_TRAIN --in $IN_CONTEXTS --topics $NUM_TOPICS --out $OUT_CLUSTERS --em $NUM_EM_ITERS --pr $NUM_PR_ITERS --scale-phrase $PR_SCALE_P --scale-context $PR_SCALE_C --threads $PR_THREADS") or die "Topic training failed.\n";
  }
}

sub label_spans_with_topics {
  my ($file) = (@_);
  print STDERR "\n!!!LABEL SPANS\n";
  my $IN_CLUSTERS = "$CLUSTER_DIR/docs.txt.gz";
  my $OUT_SPANS = "$LABELED_DIR/labeled_spans.txt";
  if (-e $OUT_SPANS) {
    print STDERR "$OUT_SPANS exists, reusing...\n";
  } else {
    my $l = "tt";
    if ($LANGUAGE eq "source") {
        $l = "ss";
    } elsif ($LANGUAGE eq "both") {
        $l = "bb";
    } else { die "Invalid language specifier $LANGUAGE\n" unless $LANGUAGE eq "target" };
    safesystem("$ZCAT $IN_CLUSTERS > $CLUSTER_DIR/clusters.txt") or die "Failed to unzip";
    safesystem("$EXTRACTOR --base_phrase_spans -i $CORPUS_CLUSTER -c $ITEMS_IN_MEMORY -L $BASE_PHRASE_MAX_SIZE -S $CONTEXT_SIZE | $S2L $CLUSTER_DIR/clusters.txt $CONTEXT_SIZE $LABEL_THRESHOLD $l > $OUT_SPANS") or die "Failed to label spans";
    unlink("$CLUSTER_DIR/clusters.txt") or warn "Failed to remove $CLUSTER_DIR/clusters.txt";
    safesystem("paste -d ' ' $CORPUS_LEX $OUT_SPANS > $LABELED_DIR/corpus.src_trg_al_label") or die "Couldn't paste";
  }
}

sub combine_labelled_spans {
    print STDERR "\n!!!COMBINING SPAN LABELS\n";
    my $IN_COARSE = "$CLUSTER_DIR_C/labeled_spans.txt";
    my $OUT_COARSE = "$CLUSTER_DIR_C/labeled_spans_c.txt";
    my $IN_FINE = "$CLUSTER_DIR_F/labeled_spans.txt";
    my $OUT_FINE = "$CLUSTER_DIR_F/labeled_spans_f.txt";
    my $OUT_SPANS = "$CLUSTER_DIR_F/labeled_spans.hier$NUM_TOPICS_COARSE-$NUM_TOPICS_FINE.txt";
    my $COARSE_EXPR = "\'s/\\(X[0-9][0-9]*\\)/\\1c/g\'";
    my $FINE_EXPR = "\'s/\\(X[0-9][0-9]*\\)/\\1f/g\'";
    if (-e $OUT_SPANS) {
        print STDERR "$OUT_SPANS exists, reusing...\n";
    } else {
        safesystem("$SED $COARSE_EXPR < $IN_COARSE > $OUT_COARSE") or die "Couldn't create coarse labels.";
        safesystem("$SED $FINE_EXPR < $IN_FINE > $OUT_FINE") or die "Couldn't create fine labels.";
        safesystem("sed -e 's/||| \\(.*\\)\$/\\1/' < $OUT_COARSE | paste -d ' ' $OUT_FINE - > $OUT_SPANS") or die "Couldn't paste coarse and fine labels.";
        safesystem("paste -d ' ' $CORPUS_LEX $OUT_SPANS > $CLUSTER_DIR_F/corpus.src_trg_al_label.hier") or die "Couldn't paste corpus";
    }
    $CLUSTER_DIR = $CLUSTER_DIR_F;
}

sub grammar_extract {
  my $LABELED = ($HIER_CAT ? "$CLUSTER_DIR_F/corpus.src_trg_al_label.hier" : "$LABELED_DIR/corpus.src_trg_al_label");
  print STDERR "\n!!!EXTRACTING GRAMMAR\n";
  my $OUTGRAMMAR = "$GRAMMAR_DIR/grammar.gz";
  if (-e $OUTGRAMMAR) {
    print STDERR "$OUTGRAMMAR exists, reusing...\n";
  } else {
    my $BACKOFF_ARG = ($BACKOFF_GRAMMAR ? "-g" : "");
    my $DEFAULT_CAT_ARG = ($DEFAULT_CAT ? "-d X" : "");
    safesystem("$EXTRACTOR -i $LABELED -c $ITEMS_IN_MEMORY -L $BASE_PHRASE_MAX_SIZE -t $NUM_TOPICS $BACKOFF_ARG $DEFAULT_CAT_ARG | $SORT_KEYS | $REDUCER -p | $GZIP > $OUTGRAMMAR") or die "Couldn't extract grammar";
  }
  return $OUTGRAMMAR;
}

sub grammar_extract_bidir {
#gzcat ex.output.gz | ./mr_stripe_rule_reduce -p -b | sort -t $'\t' -k 1 | ./mr_stripe_rule_reduce | gzip > phrase-table.gz
  my $LABELED = ($HIER_CAT ? "$CLUSTER_DIR_F/corpus.src_trg_al_label.hier" : "$LABELED_DIR/corpus.src_trg_al_label");
  print STDERR "\n!!!EXTRACTING GRAMMAR\n";
  my $OUTGRAMMAR = "$GRAMMAR_DIR/grammar.bidir.gz";
  if (-e $OUTGRAMMAR) {
    print STDERR "$OUTGRAMMAR exists, reusing...\n";
  } else {
    my $BACKOFF_ARG = ($BACKOFF_GRAMMAR ? "-g" : "");
    safesystem("$EXTRACTOR -i $LABELED -c $ITEMS_IN_MEMORY -L $BASE_PHRASE_MAX_SIZE -b -t $NUM_TOPICS $BACKOFF_ARG | $SORT_KEYS | $REDUCER -p -b | $SORT_KEYS | $REDUCER | $GZIP > $OUTGRAMMAR") or die "Couldn't extract grammar";
  }
  return $OUTGRAMMAR;
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

