#!/usr/bin/perl -w
use strict;
my $T="\t";
my %d;
sub info {
    local $,=' ';
    print STDERR @_,"\n";
}
for my $n (0..$#ARGV) {
    open F,'<',$ARGV[$n];
    info($n,$ARGV[$n]);
    while(<F>) {
        my ($x,$f,$r)=split ' ',$_,3;
        $d{$x}->[$n]=$f
    }
}
for (sort keys %d) {
    my @f=map { $T.(defined($_)?$_:'x') } @{$d{$_}};
    print $_,@f,"\n";
}
