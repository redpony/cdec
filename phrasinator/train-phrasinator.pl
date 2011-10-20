#!/usr/bin/perl -w
use strict;
my $script_dir; BEGIN { use Cwd qw/ abs_path cwd /; use File::Basename; $script_dir = dirname(abs_path($0)); push @INC, $script_dir; }
use Getopt::Long;
use File::Spec qw (rel2abs);

my $DECODER = "$script_dir/../decoder/cdec";
my $TRAINER = "$script_dir/gibbs_train_plm_notables";

die "Can't find $TRAINER" unless -f $TRAINER;
die "Can't execute $TRAINER" unless -x $TRAINER;

if (!GetOptions(
	"decoder=s" => \$DECODER,
)) { usage(); }

die "Can't find $DECODER" unless -f $DECODER;
die "Can't execute $DECODER" unless -x $DECODER;
if (scalar @ARGV != 2) { usage(); }
my $INFILE = shift @ARGV;
my $OUTDIR = shift @ARGV;
$OUTDIR = File::Spec->rel2abs($OUTDIR);
print STDERR "      Input file: $INFILE\n";
print STDERR "Output directory: $OUTDIR\n";
open F, "<$INFILE" or die "Failed to open $INFILE for reading: $!";
close F;
die "Please remove existing directory $OUTDIR\n" if (-f $OUTDIR || -d $OUTDIR);

my $CMD = "mkdir $OUTDIR";
safesystem($CMD) or die "Failed to create directory $OUTDIR\n$!";

my $grammar="$OUTDIR/grammar.gz";
my $weights="$OUTDIR/weights";
$CMD = "$TRAINER -w $weights -g $grammar -i $INFILE";
safesystem($CMD) or die "Failed to train model!\n";
my $cdecini = "$OUTDIR/cdec.ini";
open C, ">$cdecini" or die "Failed to open $cdecini for writing: $!";

print C <<EOINI;
quiet=true
formalism=scfg
grammar=$grammar
add_pass_through_rules=true
weights=$OUTDIR/weights
EOINI

close C;

print <<EOT;

Model trained successfully.  Text can be decoded into phrasal units with
the following command:

  $DECODER -c $OUTDIR/cdec.ini < FILE.TXT

EOT
exit(0);

sub usage {
  print <<EOT;
Usage: $0 [options] INPUT.TXT OUTPUT-DIRECTORY

  Infers a phrasal segmentation model from the tokenized text in INPUT.TXT
  and writes it to OUTPUT-DIRECTORY/ so that it can be applied to other
  text or have its granularity altered.

EOT
  exit(1);
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

