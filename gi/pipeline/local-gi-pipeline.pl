#!/usr/bin/perl -w
use strict;
my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path cwd /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }

use Getopt::Long "GetOptions";

my $GZIP = 'gzip';
my $ZCAT = 'gunzip -c';
my $BASE_PHRASE_MAX_SIZE = 10;
my $COMPLETE_CACHE = 1;
my $ITEMS_IN_MEMORY = 3000000;  # cache size in extractors
my $NUM_TOPICS = 50;
my $NUM_SAMPLES = 100;
my $CONTEXT_SIZE = 1;
my $BIDIR = 1;

my $EXTOOLS = "$SCRIPT_DIR/../../extools";
die "Can't find extools: $EXTOOLS" unless -e $EXTOOLS && -d $EXTOOLS;
my $PYPTOOLS = "$SCRIPT_DIR/../pyp-topics/src";
die "Can't find extools: $PYPTOOLS" unless -e $PYPTOOLS && -d $PYPTOOLS;
my $PYPSCRIPTS = "$SCRIPT_DIR/../pyp-topics/scripts";
die "Can't find extools: $PYPSCRIPTS" unless -e $PYPSCRIPTS && -d $PYPSCRIPTS;
my $REDUCER = "$EXTOOLS/mr_stripe_rule_reduce";
my $C2D = "$PYPSCRIPTS/contexts2documents.py";
my $S2L = "$PYPSCRIPTS/spans2labels.py";

my $PYP_TOPICS_TRAIN="$PYPTOOLS/pyp-topics-train";

my $SORT_KEYS = "$SCRIPT_DIR/scripts/sort-by-key.sh";
my $EXTRACTOR = "$EXTOOLS/extractor";
my $FILTER = "$EXTOOLS/filter_grammar";
my $SCORER = "$EXTOOLS/score_grammar";
my $TOPIC_TRAIN = "$PYPTOOLS/pyp-topics-train";

assert_exec($SORT_KEYS, $REDUCER, $EXTRACTOR, $FILTER, $SCORER, $PYP_TOPICS_TRAIN, $S2L, $C2D, $TOPIC_TRAIN);

my $OUTPUT = './giwork';

usage() unless &GetOptions('base_phrase_max_size=i' => \$BASE_PHRASE_MAX_SIZE,
                           'output=s' => \$OUTPUT,
                           'topics=i' => \$NUM_TOPICS,
                           'trg_context=i' => \$CONTEXT_SIZE,
                           'samples=i' => \$NUM_SAMPLES,
                          );

mkdir($OUTPUT);
die "Couldn't create output direction: $OUTPUT" unless -d $OUTPUT;
print STDERR "OUTPUT DIRECTORY: $OUTPUT\n";

usage() unless scalar @ARGV == 1;
my $CORPUS = $ARGV[0];
open F, "<$CORPUS" or die "Can't read $CORPUS: $!"; close F;

extract_context();
# contexts_to_documents();
topic_train();
label_spans_with_topics();
my $res;
if ($BIDIR) {
  $res = grammar_extract_bidir();
} else {
  $res = grammar_extract();
}
print STDERR "\n!!!COMPLETE!!!\n";
print STDERR "GRAMMAR: $res\n\nYou should probably run:\n\n   $SCRIPT_DIR/filter-for-test-set.pl $CORPUS $res TESTSET.TXT > filtered-grammar.scfg\n\n";
exit 0;







sub usage {
  print <<EOT;

Usage: $0 [OPTIONS] corpus.fr-en-al

Induces a grammar using Pitman-Yor topic modeling.

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
 my $OUT_CONTEXTS = "$OUTPUT/context.txt.gz";
 if (-e $OUT_CONTEXTS) {
   print STDERR "$OUT_CONTEXTS exists, reusing...\n";
 } else {
   my $cmd = "$EXTRACTOR -i $CORPUS -c $ITEMS_IN_MEMORY -L $BASE_PHRASE_MAX_SIZE -C -S $CONTEXT_SIZE | $SORT_KEYS | $REDUCER | $GZIP > $OUT_CONTEXTS";
   if ($COMPLETE_CACHE) {
     print STDERR "COMPLETE_CACHE is set: removing memory limits on cache.\n";
     $cmd = "$EXTRACTOR -i $CORPUS -c 0 -L $BASE_PHRASE_MAX_SIZE -C -S $CONTEXT_SIZE | $SORT_KEYS | $GZIP > $OUT_CONTEXTS";
   }
   safesystem($cmd) or die "Failed to extract contexts.";
  }
}

sub contexts_to_documents {
 print STDERR "\n!!!CONTEXT TO DOCUMENTS\n"; 
 my $IN_CONTEXTS = "$OUTPUT/context.txt.gz";
 my $OUT_DOCS = "$OUTPUT/ctx.num.gz";
 if (-e $OUT_DOCS) {
   print STDERR "$OUT_DOCS exists, reusing...\n";
 } else {
   safesystem("$ZCAT $IN_CONTEXTS | $C2D $OUTPUT/contexts.index $OUTPUT/phrases.index | $GZIP > $OUT_DOCS") or die;
 }
}

sub topic_train {
  print STDERR "\n!!!TRAIN PYP TOPICS\n";
# my $IN_DOCS = "$OUTPUT/ctx.num.gz";
  my $IN_CONTEXTS = "$OUTPUT/context.txt.gz";
  my $OUT_CLUSTERS = "$OUTPUT/docs.txt.gz";
  if (-e $OUT_CLUSTERS) {
    print STDERR "$OUT_CLUSTERS exists, reusing...\n";
  } else {
    safesystem("$TOPIC_TRAIN --contexts $IN_CONTEXTS --backoff-type simple -t $NUM_TOPICS -s $NUM_SAMPLES -o $OUT_CLUSTERS -w /dev/null") or die "Topic training failed.\n";
#   safesystem("$TOPIC_TRAIN -d $IN_DOCS -t $NUM_TOPICS -s $NUM_SAMPLES -o $OUT_CLUSTERS -w /dev/null") or die "Topic training failed.\n";
  }
}

sub label_spans_with_topics {
  my ($file) = (@_);
  print STDERR "\n!!!LABEL SPANS\n";
  my $IN_CLUSTERS = "$OUTPUT/docs.txt.gz";
  my $OUT_SPANS = "$OUTPUT/labeled_spans.txt";
  if (-e $OUT_SPANS) {
    print STDERR "$OUT_SPANS exists, reusing...\n";
  } else {
    safesystem("$ZCAT $IN_CLUSTERS > $OUTPUT/clusters.txt") or die "Failed to unzip";
    safesystem("$EXTRACTOR --base_phrase_spans -i $CORPUS -c $ITEMS_IN_MEMORY -L $BASE_PHRASE_MAX_SIZE -S $CONTEXT_SIZE | $S2L $OUTPUT/phrases.index $OUTPUT/contexts.index $OUTPUT/clusters.txt > $OUT_SPANS") or die "Failed to label spans";
    unlink("$OUTPUT/clusters.txt") or warn "Failed to remove $OUTPUT/clusters.txt";
    safesystem("paste -d ' ' $CORPUS $OUT_SPANS > $OUTPUT/corpus.src_trg_al") or die "Couldn't paste";
  }
}

sub grammar_extract {
  my $LABELED = "$OUTPUT/corpus.src_trg_al";
  print STDERR "\n!!!EXTRACTING GRAMMAR\n";
  my $OUTGRAMMAR = "$OUTPUT/grammar.gz";
  if (-e $OUTGRAMMAR) {
    print STDERR "$OUTGRAMMAR exists, reusing...\n";
  } else {
    safesystem("$EXTRACTOR -i $LABELED -c $ITEMS_IN_MEMORY -L $BASE_PHRASE_MAX_SIZE | $SORT_KEYS | $REDUCER -p | $GZIP > $OUTGRAMMAR") or die "Couldn't extract grammar";
  }
  return $OUTGRAMMAR;
}

sub grammar_extract_bidir {
#gzcat ex.output.gz | ./mr_stripe_rule_reduce -p -b | sort -t $'\t' -k 1 | ./mr_stripe_rule_reduce | gzip > phrase-table.gz
  my $LABELED = "$OUTPUT/corpus.src_trg_al";
  print STDERR "\n!!!EXTRACTING GRAMMAR\n";
  my $OUTGRAMMAR = "$OUTPUT/grammar.bidir.gz";
  if (-e $OUTGRAMMAR) {
    print STDERR "$OUTGRAMMAR exists, reusing...\n";
  } else {
    safesystem("$EXTRACTOR -i $LABELED -c $ITEMS_IN_MEMORY -L $BASE_PHRASE_MAX_SIZE -b | $SORT_KEYS | $REDUCER -p -b | $SORT_KEYS | $REDUCER | $GZIP > $OUTGRAMMAR") or die "Couldn't extract grammar";
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

