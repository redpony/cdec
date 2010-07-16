#!/usr/bin/perl

while(<>) {
    chomp;
    $|=1;
    print (scalar reverse($_));
    print "\n";
}
