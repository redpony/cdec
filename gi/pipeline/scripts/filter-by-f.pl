#!/usr/bin/perl -w
use strict;

my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR; }

my $REKEY="$SCRIPT_DIR/rekey.pl";
my $REFILTER="$SCRIPT_DIR/refilter.pl";
my $SORT="$SCRIPT_DIR/sort-by-key.sh";
assert_exec($REKEY, $REFILTER, $SORT);

die "Usage: $0 ingrammar.gz outgrammar.gz\n" unless scalar @ARGV == 2;
die unless $ARGV[0] =~ /\.gz$/;
die unless $ARGV[1] =~ /\.gz$/;
die if $ARGV[0] eq $ARGV[1];
die "Can't find $ARGV[0]" unless -f $ARGV[0];

my $cmd = "gunzip -c $ARGV[0] | $REKEY | $SORT | $REFILTER | gzip > $ARGV[1]";
safesystem($ARGV[1], $cmd) or die "Filtering failed";
exit 0;

sub assert_exec {
  my @files = @_;
  for my $file (@files) {
    die "Can't find $file - did you run make?\n" unless -e $file;
    die "Can't execute $file" unless -e $file;
  }
};

sub safesystem {
  my $output = shift @_;
  print STDERR "Executing: @_\n";
  system(@_);
  if ($? == -1) {
      print STDERR "ERROR: Failed to execute: @_\n  $!\n";
      if (defined $output && -e $output) { printf STDERR "Removing $output\n"; `rm -rf $output`; }
      exit(1);
  }
  elsif ($? & 127) {
      printf STDERR "ERROR: Execution of: @_\n  died with signal %d, %s coredump\n",
          ($? & 127),  ($? & 128) ? 'with' : 'without';
      if (defined $output && -e $output) { printf STDERR "Removing $output\n"; `rm -rf $output`; }
      exit(1);
  }
  else {
    my $exitcode = $? >> 8;
    if ($exitcode) {
      print STDERR "Exit code: $exitcode\n";
      if (defined $output && -e $output) { printf STDERR "Removing $output\n"; `rm -rf $output`; }
    }
    return ! $exitcode;
  }
}

