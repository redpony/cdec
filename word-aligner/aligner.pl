#!/usr/bin/perl -w
use strict;

my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path getcwd /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }
use Getopt::Long;
my $training_dir = "$SCRIPT_DIR/../training";
die "Can't find training dir: $training_dir" unless -d $training_dir;

my $mkcls = '/Users/redpony/software/giza/giza-pp/mkcls-v2/mkcls';
my $num_classes = 50;
my $nodes = 40;
my $pmem = "2500mb";
my $DECODER = "cdec";
GetOptions("cdec=s" => \$DECODER,
           "jobs=i" => \$nodes,
           "pmem=s" => \$pmem,
           "mkcls=s" => \$mkcls,
          ) or usage();
usage() unless (scalar @ARGV == 3);
die "Cannot find mkcls (specify with --mkcls=/path/to/mkcls) at $mkcls\n" unless -f $mkcls;
die "Cannot execute mkcls at $mkcls\n" unless -x $mkcls;

my $in_file = shift @ARGV;
my $m4 = shift @ARGV;
my $im4 = shift @ARGV;
die "Can't find model4: $m4" unless -f $m4;
die "Can't find inverse model4: $im4" unless -f $im4;

die "Expected format corpus.l1-l2 where l1 & l2 are two-letter abbreviations\nfor the source and target language respectively\n" unless ($in_file =~ /^.+\.([a-z][a-z])-([a-z][a-z])$/);
my $f_lang = $1;
my $e_lang = $2;

print STDERR "Source language: $f_lang\n";
print STDERR "Target language: $e_lang\n";
print STDERR "  Model 4 align: $m4\n";
print STDERR "InModel 4 align: $im4\n";
print STDERR " Using mkcls in: $mkcls\n\n";
die "Don't have an orthographic normalizer for $f_lang\n" unless -f "$SCRIPT_DIR/ortho-norm/$f_lang.pl";
die "Don't have an orthographic normalizer for $e_lang\n" unless -f "$SCRIPT_DIR/ortho-norm/$e_lang.pl";

my @stages = qw(nopos relpos markov);
my @directions = qw(f-e e-f);

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
  my $prev_stage = undef;
  for my $stage (@stages) {
    push @targets, "$stage-$direction";
    make_stage($stage, $direction, $prev_stage);
    $prev_stage = $stage;
  }
}

open TOPLEVEL, ">$align_dir/Makefile" or die "Can't write $align_dir/Makefile: $!";

print TOPLEVEL <<EOT;
E_LANG = $e_lang
F_LANG = $f_lang
SCRIPT_DIR = $SCRIPT_DIR
TRAINING_DIR = $training_dir
MKCLS = $mkcls
NCLASSES = $num_classes
GIZAALIGN = $m4
INVGIZAALIGN = $im4

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
  my ($stage, $direction, $prev_stage) = @_;
  my $stage_dir = "$align_dir/model-$direction";
  my $first = $direction;
  $first =~ s/^(.+)-.*$/$1/;
  mkdir $stage_dir;
  my $RELPOS = "feature_function=RelativeSentencePosition $align_dir/grammars/corpus.class.$first\n";
  open CDEC, ">$stage_dir/cdec.$stage.ini" or die;
  print CDEC <<EOT;
formalism=lextrans
intersection_strategy=full
grammar=$align_dir/grammars/corpus.$direction.lex-grammar.gz
feature_function=LexicalPairIdentity
feature_function=InputIdentity
feature_function=OutputIdentity
EOT
  if ($stage =~ /relpos/) {
    print CDEC "$RELPOS\n";
  } elsif ($stage =~ /markov/) {
    print CDEC "$RELPOS\n";
    print CDEC "feature_function=MarkovJump\n";
    print CDEC "feature_function=MarkovJumpFClass $align_dir/grammars/corpus.class.$first\n";
    print CDEC "feature_function=SourceBigram\n";
    print CDEC "feature_function=SourcePOSBigram $align_dir/grammars/corpus.class.$first\n";
  }
  close CDEC;
}

sub usage {
  die <<EOT;

Usage: $0 [OPTIONS] training_corpus.fr-en giza.en-fr.A3 giza.fr-en.A3

EOT
}
