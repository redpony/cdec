#!/usr/bin/perl -w
use strict;

my ($f_classes) = @ARGV;

die "Usage: $0 f-classes.file" unless $f_classes && -f $f_classes;

print <<EOT;
MarkovJump 0
RelativeSentencePosition 0
EOT

# !	8
# "	11
# 's	18

my %dcats = ();
$dcats{'BOS'} = 1;
$dcats{'EOS'} = 1;

open FC, "<$f_classes" or die;
while(<FC>) {
  chomp;
  my ($x, $cat) = split /\s+/;
  $dcats{$cat} = 1;
}

my @cats = sort keys %dcats;

for (my $i=0; $i < scalar @cats; $i++) {
  my $c1 = $cats[$i];
  for (my $j=0; $j < scalar @cats; $j++) {
    my $c2 = $cats[$j];
    print "SP:${c1}_${c2} 0\n";
  }
}

