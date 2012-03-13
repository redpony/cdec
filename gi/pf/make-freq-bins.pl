#!/usr/bin/perl -w
use strict;

my $BASE = 6;
my $CUTOFF = 3;

my %d;
my $num = 0;
while(<>){
 chomp;
 my @words = split /\s+/;
 for my $w (@words) {$d{$w}++; $num++;}
}

my @vocab = sort {$d{$b} <=> $d{$a}} keys %d;

for (my $i=0; $i<scalar @vocab; $i++) {
  my $most = $d{$vocab[$i]};
  my $least = 1;

  my $nl = -int(log($most / $num) / log($BASE) + $CUTOFF);
  if ($nl < 0) { $nl = 0; }
  print "$vocab[$i] $nl\n"
}


