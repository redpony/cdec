#!perl

use warnings;
use strict;

my $grammar_file = $ARGV[0];

my %nt_count; #maps nt--> count rules whose lhs is nt 

open(G, "<$grammar_file") or die "Can't open file $grammar_file";

while (<G>){

    chomp();

    s/\|\|\|.*//g;
    s/\s//g;

    $nt_count{$_}++;
}


close (G);

open(G, "<$grammar_file") or die "Can't open file $grammar_file";

while (<G>){

    chomp();

    (my $nt = $_) =~ s/\|\|\|.*//g;
    $nt =~ s/\s//g;

    s/(.+\|\|\|.+\|\|\|.+\|\|\|).+/$1/g;
    print $_ . " MinusLogP=" .(log($nt_count{$nt})) ."\n";
}
