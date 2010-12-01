#!/usr/bin/perl -w
use strict;

my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path getcwd /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }
use Getopt::Long;
my $training_dir = "$SCRIPT_DIR/../training";
die "Can't find training dir: $training_dir" unless -d $training_dir;

my $mkcls = '/Users/cdyer/software/giza-pp/mkcls-v2/mkcls';
my $num_classes = 50;
my $nodes = 40;
my $TRAINING_ITERATIONS = 2000;
my $pmem = "2500mb";
my $DECODER = "cdec";
GetOptions("cdec=s" => \$DECODER,
           "jobs=i" => \$nodes,
           "pmem=s" => \$pmem,
           "mkcls=s" => \$mkcls,
          ) or usage();
usage() unless (scalar @ARGV == 1);
die "Cannot find mkcls (specify with --mkcls=/path/to/mkcls) at $mkcls\n" unless -f $mkcls;
die "Cannot execute mkcls at $mkcls\n" unless -x $mkcls;

my $in_file = shift @ARGV;

die "Expected format corpus.l1-l2 where l1 & l2 are two-letter abbreviations\nfor the source and target language respectively\n" unless ($in_file =~ /^.+\.([a-z][a-z])-([a-z][a-z])$/);
my $f_lang = $1;
my $e_lang = $2;

print STDERR "Source language: $f_lang\n";
print STDERR "Target language: $e_lang\n";
print STDERR " Using mkcls in: $mkcls\n\n";
die "Don't have an orthographic normalizer for $f_lang\n" unless -f "$SCRIPT_DIR/ortho-norm/$f_lang.pl";
die "Don't have an orthographic normalizer for $e_lang\n" unless -f "$SCRIPT_DIR/ortho-norm/$e_lang.pl";

my @directions = qw(f-e);

my $corpus = 'c';

my $cwd = getcwd();
my $align_dir = "$cwd/talign";

mkdir $align_dir;
mkdir "$align_dir/grammars";
open IN, "<$in_file" or die "Can't read $in_file: $!";
open E, ">$align_dir/grammars/corpus.e" or die "Can't write: $!";
open F, ">$align_dir/grammars/corpus.f" or die "Can't write: $!";
while(<IN>) {
  chomp;
  my ($f, $e) = split / \|\|\| /;
  die "Bad format, excepted ||| separated line" unless defined $f && defined $e;
  print F "$f\n";
  print E "$e\n";
}
close F;
close E;
close IN;
`cp $SCRIPT_DIR/makefiles/makefile.grammars $align_dir/grammars/Makefile`;
die unless $? == 0;

my @targets = qw(grammars);

for my $direction (@directions) {
  push @targets, "model-$direction";
  make_stage($direction);
}

open TOPLEVEL, ">$align_dir/Makefile" or die "Can't write $align_dir/Makefile: $!";

print TOPLEVEL <<EOT;
E_LANG = $e_lang
F_LANG = $f_lang
SCRIPT_DIR = $SCRIPT_DIR
TRAINING_DIR = $training_dir
MKCLS = $mkcls
NCLASSES = $num_classes

TARGETS = @targets
PTRAIN = \$(TRAINING_DIR)/cluster-ptrain.pl --restart_if_necessary
PTRAIN_PARAMS = --gaussian_prior --sigma_squared 1.0 --max_iteration 15

export

all:
	\@failcom='exit 1'; \\
	list='\$(TARGETS)'; for subdir in \$\$list; do \\
	echo "Making \$\$subdir ..."; \\
	(cd \$\$subdir && \$(MAKE)) || eval \$\$failcom; \\
	done

clean:
	\@failcom='exit 1'; \\
	list='\$(TARGETS)'; for subdir in \$\$list; do \\
	echo "Making \$\$subdir ..."; \\
	(cd \$\$subdir && \$(MAKE) clean) || eval \$\$failcom; \\
	done
EOT
close TOPLEVEL;

print STDERR "Created alignment task. chdir to talign/ then type make.\n\n";
exit 0;

sub make_stage {
  my ($direction) = @_;
  my $stage_dir = "$align_dir/model-$direction";
  my $first = $direction;
  my $second = $direction;
  $first =~ s/^(.+)-.*$/$1/;
  $second =~ s/^.+-(.+)$/$1/;
  mkdir $stage_dir;
  open CDEC, ">$stage_dir/cdec.ini" or die "Can't write $stage_dir/cdec.ini: $!";
  print CDEC <<EOT;
formalism=lextrans
lextrans_use_null=true
intersection_strategy=full

grammar=$align_dir/grammars/corpus.$direction.lex-grammar.gz
# grammar=$align_dir/grammars/freq_grammar.$direction.gz
# per_sentence_grammar_file=$align_dir/grammars/psg.$direction

feature_function=WordPairFeatures $align_dir/grammars/wordpairs.$direction.features.gz
feature_function=LexicalPairIdentity
feature_function=LexicalPairIdentity C $align_dir/grammars/corpus.class.$first $align_dir/grammars/voc2class.$second
feature_function=LexicalPairIdentity S $align_dir/grammars/corpus.stemmed.$first $align_dir/grammars/${second}stem.map
feature_function=InputIdentity
feature_function=OutputIdentity
feature_function=RelativeSentencePosition $align_dir/grammars/corpus.class.$first
# the following two are deprecated
feature_function=MarkovJump +b
feature_function=MarkovJumpFClass $align_dir/grammars/corpus.class.$first
feature_function=SourceBigram
# following is deprecated- should reuse SourceBigram the way LexicalPairIdentity does
feature_function=SourcePOSBigram $align_dir/grammars/corpus.class.$first
EOT
  close CDEC;
  open AGENDA, ">$stage_dir/agenda.txt" or die "Can't write $stage_dir/agenda.txt: $!";
  print AGENDA "cdec.ini $TRAINING_ITERATIONS\n";
  close AGENDA;
}

sub usage {
  die <<EOT;

Usage: $0 [OPTIONS] training_corpus.fr-en

EOT
}
