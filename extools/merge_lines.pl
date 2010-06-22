#!/usr/bin/perl -w
use strict;

if (scalar @ARGV < 2) {
  die "Usage: $0 file1.txt file2.txt ...\n\n  Concatenate the nth line of each input file. All files\n  must be the same length.\n\n";
}

my @fhs=();
for my $file (@ARGV) {
  my $fh;
  open $fh, "<$file" or die "Can't read $file: $!\n";
  push @fhs, $fh;
}

my $first = shift @fhs;

while(my $x = <$first>) {
  my $ind = 0;
  chomp $x;
  my @fields = ($x);
  for my $fh (@fhs) {
    $ind++;
    $x = <$fh>;
    die "ERROR: Mismatched number of lines: $ARGV[$ind]\n" unless $x;
    chomp $x;
    push @fields, $x;
  }
  print join ' ||| ', @fields;
  print "\n";
}
my $ind = 0;
for my $fh (@fhs) {
  $ind++;
  my $x=<$fh>;
  die "ERROR: $ARGV[$ind] has extra lines!\n" if $x;
}

exit 0;

for my $fh (@fhs) {
  close $fh;
}

