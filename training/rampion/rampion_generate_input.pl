#!/usr/bin/perl -w
use strict;

die "Usage: $0 HG_DIR\n" unless scalar @ARGV == 1;
my $d = shift @ARGV;
die "Can't find directory $d" unless -d $d;

opendir(DIR, $d) or die "Can't read $d: $!";
my @hgs = grep { /\.gz$/ } readdir(DIR);
closedir DIR;

for my $hg (@hgs) {
  my $file = $hg;
  my $id = $hg;
  $id =~ s/(\.json)?\.gz//;
  print "$d/$file $id\n";
}

