#!/usr/bin/perl -w
use strict;

# 1	在	_	10	_	_	4	X	_	_
# 2	门厅	_	3	_	_	1	X	_	_
# 3	下面	_	23	_	_	4	X	_	_
# 4	。	_	45	_	_	0	X	_	_

my @ldeps;
my @rdeps;
@ldeps=(); for (my $i =0; $i <1000; $i++) { push @ldeps, []; }
@rdeps=(); for (my $i =0; $i <1000; $i++) { push @rdeps, []; }
my $rootcat = 0;
my @cats = ('S');
my $len = 0;
my @noposcats = ('S');
while(<>) {
  chomp;
  if (/^\s*$/) {
    write_cfg($len);
    $len = 0;
    @cats=('S');
    @noposcats = ('S');
    @ldeps=(); for (my $i =0; $i <1000; $i++) { push @ldeps, []; }
    @rdeps=(); for (my $i =0; $i <1000; $i++) { push @rdeps, []; }
    next;
  }
  $len++;
  my ($pos, $word, $d1, $xcat, $d2, $d3, $headpos, $deptype) = split /\s+/;
  my $cat = "C$xcat";
  my $catpos = $cat . "_$pos";
  push @cats, $catpos;
  push @noposcats, $cat;
  print "[$catpos] ||| $word ||| $word ||| Word=1\n";
  if ($headpos == 0) { $rootcat = $pos; }
  if ($pos < $headpos) {
    push @{$ldeps[$headpos]}, $pos;
  } else {
    push @{$rdeps[$headpos]}, $pos;
  }
}

sub write_cfg {
  my $len = shift;
  for (my $i = 1; $i <= $len; $i++) {
    my @lds = @{$ldeps[$i]};
    for my $ld (@lds) {
      print "[$cats[$i]] ||| [$cats[$ld],1] [$cats[$i],2] ||| [1] [2] ||| $noposcats[$ld]_$noposcats[$i]=1\n";
    }
    my @rds = @{$rdeps[$i]};
    for my $rd (@rds) {
      print "[$cats[$i]] ||| [$cats[$i],1] [$cats[$rd],2] ||| [1] [2] ||| $noposcats[$i]_$noposcats[$rd]=1\n";
    }
  }
  print "[S] ||| [$cats[$rootcat],1] ||| [1] ||| TOP=1\n";
}

