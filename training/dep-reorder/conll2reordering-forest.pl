#!/usr/bin/perl -w
use strict;

my $script_dir; BEGIN { use Cwd qw/ abs_path cwd /; use File::Basename; $script_dir = dirname(abs_path($0)); push @INC, $script_dir; }
my $FIRST_CONV = "$script_dir/scripts/conll2simplecfg.pl";
my $CDEC = "$script_dir/../../decoder/cdec";

our $tfile1 = "grammar1.$$";
our $tfile2 = "text.$$";

die "Usage: $0 parses.conll\n" unless scalar @ARGV == 1;
open C, "<$ARGV[0]" or die "Can't read $ARGV[0]: $!";

END { unlink $tfile1; unlink "$tfile1.cfg"; unlink $tfile2; }

my $first = 1;
open T, ">$tfile1" or die "Can't write $tfile1: $!";
my $lc = 0;
my $flag = 0;
my @words = ();
while(<C>) {
  print T;
  chomp;
  if (/^$/) {
    if ($first) { $first = undef; } else { if ($flag) { print "\n"; $flag = 0; } }
    $first = undef;
    close T;
    open SO, ">$tfile2" or die "Can't write $tfile2: $!";
    print SO "@words\n";
    close SO;
    @words=();
    `$FIRST_CONV < $tfile1 > $tfile1.cfg`;
    if ($? != 0) {
      die "Error code: $?";
    }
    my $cfg = `$CDEC -n -S 10000 -f scfg -g $tfile1.cfg -i $tfile2 --show_cfg_search_space 2>/dev/null`;
    if ($? != 0) {
      die "Error code: $?";
    }
    my @rules = split /\n/, $cfg;
    shift @rules; # get rid of output
    for my $rule (@rules) {
      my ($lhs, $f, $e, $feats) = split / \|\|\| /, $rule;
      $f =~ s/,\d\]/\]/g;
      $feats = 'TOP=1' unless $feats;
      if ($lhs =~ /\[Goal_\d+\]/) { $lhs = '[S]'; }
      print "$lhs ||| $f ||| $feats\n";
      if ($e eq '[1] [2]') {
        my ($a, $b) = split /\s+/, $f;
        $feats =~ s/=1$//;
        my ($x, $y) = split /_/, $feats;
        print "$lhs ||| $b $a ||| ${y}_$x=1\n";
      }
      $flag = 1;
    }
    open T, ">$tfile1" or die "Can't write $tfile1: $!";
    $lc = -1;
  } else {
    my ($ind, $word, @dmmy) = split /\s+/;
    push @words, $word;
  }
  $lc++;
}
close T;

