#!/usr/bin/perl -w
use strict;

my $ADD_FCLASS_JUMP = 1;
my $ADD_MODEL2_BINARY = 0;
my $ADD_FC_RELPOS = 1;

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

my $added = 0;
for (my $i=0; $i < scalar @cats; $i++) {
  my $c1 = $cats[$i];
  for (my $j=0; $j < scalar @cats; $j++) {
    my $c2 = $cats[$j];
    print "SP:${c1}_${c2} 0\n";
    $added++;
  }
}

for (my $ss=1; $ss < 100; $ss++) {
  if ($ADD_FCLASS_JUMP) {
    for (my $i=0; $i < scalar @cats; $i++) {
      my $cat = $cats[$i];
      for (my $j = -$ss; $j <= $ss; $j++) {
        print "Jump_FL:${ss}_FC:${cat}_J:$j 0\n";
        $added++;
      }
    }
  }
  if ($ADD_MODEL2_BINARY) {
    # M2_FL:8_SI:3_TI:2=1
    for (my $i = 0; $i < $ss; $i++) {
      for (my $j = 0; $j < 100; $j++) {
        print "M2FL:${ss}:TI:${j}_SI:${i} 0\n";
        $added++;
      }
    }
  }
}
if ($ADD_FC_RELPOS) {
  #RelPos_FC:11
  for (my $i=0; $i < scalar @cats; $i++) {
    my $cat = $cats[$i];
    print "RelPos_FC:$cat 0\n";
    $added++;
  }
}

print STDERR "Added $added weights\n";
