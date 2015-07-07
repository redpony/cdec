#!/usr/bin/perl -w
use strict;
my $script_dir; BEGIN { use Cwd qw/ abs_path cwd /; use File::Basename; $script_dir = dirname(abs_path($0)); push @INC, $script_dir; }

use IPC::Run3;
use File::Temp qw ( tempdir );
my $TEMP_DIR = tempdir( CLEANUP => 1 );

#my $cwd = cwd();
#die "Sanity failed: $cwd" unless -d $cwd;
my $DECODER = "$script_dir/../decoder/cdec";
my $FILTER = "$script_dir/tools/filter-stderr.pl";
my $COMPARE_STATS = "$script_dir/tools/compare-statistics.pl";
my $XDIFF = "$script_dir/tools/flex-diff.pl";

die "Can't find $DECODER" unless -f $DECODER;
die "Can't execute $DECODER" unless -x $DECODER;
die "Can't find $FILTER" unless -f $FILTER;
die "Can't execute $FILTER" unless -x $FILTER;
die "Can't find $COMPARE_STATS" unless -f $COMPARE_STATS;
die "Can't execute $COMPARE_STATS" unless -x $COMPARE_STATS;
die "Can't execute $XDIFF" unless -x $XDIFF;

my $TEST_DIR = "$script_dir/system_tests";
opendir DIR, $TEST_DIR or die "Can't open $TEST_DIR: $!";
#my @test_dirs = grep { /^\./ && −d "$some_dir/$_" } readdir(DIR);
my @tests = grep { !/^\./ && -d "$TEST_DIR/$_" } readdir(DIR);
closedir DIR;

print STDERR " DECODER: $DECODER\n";
print STDERR "   TESTS: @tests\n";
print STDERR "TEMP DIR: $TEMP_DIR\n";

my $FAIL = 0;
my $PASS = 0;
for my $test (@tests) {
  print "TEST: $test\n";
  chdir "$TEST_DIR/$test" or die "Can't chdir to $TEST_DIR/$test: $!";
  my $CMD = "$DECODER";
  unless (-f 'gold.statistics') {
    print "  missing gold.statistics -- SKIPPING\n";
    $FAIL++;
    next;
  }
  unless (-f 'gold.stdout') {
    print "  missing gold.stdout -- SKIPPING\n";
    $FAIL++;
    next;
  }
  if (-f 'cdec.ini') {
    $CMD .= ' -c cdec.ini';
  }
  if (-f 'weights') {
    $CMD .= ' -w weights';
  }
  if (-f 'input.txt') {
    $CMD .= ' -i input.txt';
  }

  run3 $CMD, \undef, "$TEMP_DIR/stdout", "$TEMP_DIR/stderr";
  if ($? != 0) {
    print STDERR "  non-zero exit! command: $CMD\n";
    $FAIL++;
  } else {
    die unless -f "$TEMP_DIR/stdout";
    my $failed = 0;
    run3 "$XDIFF gold.stdout $TEMP_DIR/stdout";
    if ($? != 0) {
      print STDERR "  FAILED differences in output!\n";
      $failed = 1;
    }
    die unless -f "$TEMP_DIR/stderr";
    run3 "$FILTER", "$TEMP_DIR/stderr", "$TEMP_DIR/test.statistics";
    if ($? != 0) {
      print STDERR "  non-zero exit: $FILTER\n";
      $FAIL++;
      next;
    }
    my @lines;
    run3 "$COMPARE_STATS gold.statistics", "$TEMP_DIR/test.statistics", \@lines;
    if (scalar @lines != 1) {
      print STDERR "  unexpected output: @lines\n";
      $FAIL++;
      next;
    }
    my $l = $lines[0]; chomp $l;
    if ($l =~ /^(\d+) (\d+)$/) {
      my $passes = $1;
      my $total = $2;
      my $pct = $passes * 100 / $total;
      $pct = sprintf "%.2f", $pct;
      
      if ($total == $passes) {
        if ($failed) {
          print "    (decoder statistics match, though)\n";
        } else {
          print "  PASSED\n";
        }
      } else {
        if ($failed) {
          print "     ($pct of decoder search statistics match)\n";
        } else {
          print "  FAILED $pct of decoder search statistics match\n";
        }
      }
    } else {
      $failed = 1;
      print STDERR "  bad format: $l\n";
    }
    if ($failed) { $FAIL++; } else { $PASS++; }
  }
}

my $TOT = $PASS + $FAIL;
print "\nSUMMARY: $PASS / $TOT TESTS PASSED\n";
if ($FAIL != 0) {
  print "  !!! THERE WERE FAILURES - DECODER IS ACTING SUSPICIOUSLY !!!\n\n";
  exit 1;
} else {
  print "\n";
  exit 0;
}

