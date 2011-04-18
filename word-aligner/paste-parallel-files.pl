#!/usr/bin/perl -w
use strict;

my @fs = ();
for my $file (@ARGV) {
  my $fh;
  open $fh, "<$file" or die "Can't open $file for reading: $!";
  push @fs, $fh;
}
my $num = scalar @fs;
die "Usage: $0 file1.txt file2.txt [...]\n" unless $num > 1;

my $first = $fs[0];
while(<$first>) {
  chomp;
  my @out = ();
  push @out, $_;
  for (my $i=1; $i < $num; $i++) {
    my $f = $fs[$i];
    my $line = <$f>;
    die "Mismatched number of lines!" unless defined $line;
    chomp $line;
    push @out, $line;
  }
  print join(' ||| ', @out) . "\n";
}

for my $fh (@fs) {
  my $x=<$fh>;
  die "Mismatched number of lines!" if defined $x;
  close $fh;
}

exit 0;

