#!/usr/bin/perl -w
use strict;

my $script_dir; BEGIN { use Cwd qw/ abs_path cwd /; use File::Basename; $script_dir = dirname(abs_path($0)); push @INC, "$script_dir/.."; }

use IPC::Run3;

# this file abstracts away from differences due to different hash
# functions that lead to different orders of features, n-best entries,
# etc.

die "Usage: $0 file1.txt file2.txt\n" unless scalar @ARGV == 2;
my $tmpa = "tmp.$$.a";
my $tmpb = "tmp.$$.b";
create_sorted($ARGV[0], $tmpa);
create_sorted($ARGV[1], $tmpb);

my $failed = 0;
run3 "diff $tmpa $tmpb";
if ($? != 0) {
  run3 "diff $ARGV[0] $ARGV[1]";
  $failed = 1;
}

unlink $tmpa;
unlink $tmpb;

exit $failed;

sub create_sorted {
  my ($in, $out) = @_;
  open A, "sort $in|" or die "Can't read $in: $!";
  open AA, ">$out" or die "Can't write $out: $!";
  while(<A>) {
    chomp;
    s/^\s*//;
    s/\s*$//;
    my @cs = split //;
    @cs = sort @cs;
    my $o = join('', @cs);
    print AA "$o\n";
  }
  close AA;
  close A;
}

