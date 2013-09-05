#!/usr/bin/env perl

use IPC::Open2;

$|++;

if (scalar(@ARGV) != 1) {
    print STDERR "usage: $0 \"CMD\"\n";
    exit(2);
}

$CMD = $ARGV[0];

while (<STDIN>) {
    s/\r\n*/\n/g;
    $PID = open2(*SOUT, *SIN, $CMD);
    print SIN "$_\n";
    close(SIN);
    $_ = <SOUT>;
    close(SOUT);
    waitpid($PID, 0);
    chomp;
    s/[\x00-\x1F]+/ /g;
    s/  +/ /g;
    s/^ //;
    s/ $//;
    print "$_\n";
}
