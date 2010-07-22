#!/usr/bin/perl -w
use strict;

use Getopt::Long "GetOptions";

my $PHRASE = 'tok';
my $CONTEXT = 'tag';

die "Usage: $0 [--phrase=tok|tag] [--context=tok|tag] < corpus" 
    unless &GetOptions('phrase=s' => \$PHRASE, 'context=s' => \$CONTEXT);

my $lno = 0;
while(my $line = <>) {
    $lno++;
    chomp $line;
    my @top = split /\t/, $line;
    die unless (scalar @top == 2); 

    my @pwords = split /\s+/, $top[0];
    foreach my $token (@pwords) {
        #print $token . "\n";
        my @parts = split /_(?!_)/, $token;
        die unless (scalar @parts == 2); 
        if ($PHRASE eq "tok") {
            $token = $parts[0]
        } elsif ($PHRASE eq "tag") {
            $token = $parts[1]
        }
    }

    my @fields = split / \|\|\| /, $top[1];
    foreach my $i (0..((scalar @fields) / 2 - 1)) {
        #print $i . ": " . $fields[2*$i] . " of " . (scalar @fields) . "\n";
        my @cwords = split /\s+/, $fields[2*$i];
        foreach my $token (@cwords) {
            #print $i . ": " . $token . "\n";
            my @parts = split /_/, $token;
            if (scalar @parts == 2) {
                if ($CONTEXT eq "tok") {
                    $token = $parts[0]
                } elsif ($CONTEXT eq "tag") {
                    $token = $parts[1]
                }
            }
        }
        $fields[2*$i] = join ' ', @cwords;
    }

    print join ' ', @pwords;
    print "\t";
    print join ' ||| ', @fields;
    print "\n";
}
