#!/usr/bin/perl -w

use strict;
use utf8;
my @ORIG_ARGV=@ARGV;
use Cwd qw(getcwd);
my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR, "$SCRIPT_DIR/../environment"; }
use LocalConfig;
use Getopt::Long;
use IPC::Open2;
use POSIX ":sys_wait_h";

my $decoder = "$SCRIPT_DIR/../decoder/cdec";
my $help;
my $cdec_ini;
my $src_file;
my $hyp_file;
my $reverse_model;
my $weights_file;
my $feature_name='NewModel';

sub catch_pipe {
  my $signame = shift;
  die "$0 received SIGPIPE: did the decoder die?\n";
}
$SIG{PIPE} = \&catch_pipe;

Getopt::Long::Configure("no_auto_abbrev");
if (GetOptions(
    "config|c=s" => \$cdec_ini,
    "weights|w=s" => \$weights_file,
    "source_file|s=s" => \$src_file,
    "feature_name|f=s" => \$feature_name,
    "hypothesis_file|h=s" => \$hyp_file,
    "reverse" => \$reverse_model,  # if true translate hyp -> src
    "decoder=s" => \$decoder,
    "help" => \$help,
) == 0 || @ARGV!=0 || $help || !$cdec_ini || !$src_file || !$hyp_file) {
  usage();
  exit;
}
die "Can't find $decoder" unless -f $decoder;
die "Can't run $decoder" unless -x $decoder;
my $weights = '';
if (defined $weights_file) {
  die "Can't read $weights_file" unless -f $weights_file;
  $weights = "-w $weights_file";
}
my $decoder_command = "$decoder -c $cdec_ini --quiet $weights --show_partition_as_translation";
print STDERR "DECODER COMMAND: $decoder_command\n";
my $cdec_pid = open2(\*CDEC_IN, \*CDEC_OUT, $decoder_command)
  or die "Couldn't run $decoder: $!";
sleep 1;

die "Can't find $cdec_ini" unless -f $cdec_ini;
open SRC, "<$src_file" or die "Can't read $src_file: $!";
open HYP, "<$hyp_file" or die "Can't read $hyp_file: $!";
binmode(SRC,":utf8");
binmode(HYP,":utf8");
binmode(STDOUT,":utf8");
my @source; while(<SRC>){chomp; push @source, $_; }
close SRC;
my $src_len = scalar @source;
print STDERR "Read $src_len sentences...\n";
binmode(CDEC_IN, ":utf8");
binmode(CDEC_OUT, ":utf8");

my $cur = undef;
my @hyps = ();
my @feats = ();
while(<HYP>) {
  chomp;
  my ($id, $hyp, $feats) = split / \|\|\| /;
  unless (defined $cur) { $cur = $id; }
  die "sentence ids in k-best list file must be between 0 and $src_len" if $id < 0 || $id > $src_len;
  if ($cur ne $id) {
    rescore($cur, $source[$cur], \@hyps, \@feats);
    $cur = $id;
    @hyps = ();
    @feats = ();
  }
  push @hyps, $hyp;
  push @feats, $feats;
}
rescore($cur, $source[$cur], \@hyps, \@feats) if defined $cur;

close CDEC_IN;
close CDEC_OUT;
close HYP;
waitpid($cdec_pid, 0);
my $status = $? >> 8;
if ($status != 0) {
  print STDERR "Decoder returned bad status!\n";
}

sub rescore {
  my ($id, $src, $rh, $rf) = @_;
  my @hyps = @$rh;
  my @feats = @$rf;
  my $nhyps = scalar @hyps;
  print STDERR "RESCORING SENTENCE id=$id (# hypotheses=$nhyps)...\n";
  for (my $i=0; $i < $nhyps; $i++) {
    if ($reverse_model) {
      print CDEC_OUT "<seg id=\"$id\">$hyps[$i] ||| $src</seg>\n";
    } else {
      print CDEC_OUT "<seg id=\"$id\">$src ||| $hyps[$i]</seg>\n";
    }
    my $score = <CDEC_IN>;
    chomp $score;
    my @words = split /\s+/, $hyps[$i];
    my $norm_score = $score / scalar @words;
    print "$id ||| $hyps[$i] ||| $feats[$i] $feature_name=$score ${feature_name}_norm=$norm_score\n";
  }
}

sub usage {
  print <<EOT;
Usage: $0 -c cdec.ini [-w cdec_weights.txt] -s source.txt -h hypothesis.nbest.txt [-f FeatureName]
EOT
  exit 0
}

