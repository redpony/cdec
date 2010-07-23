#!/usr/bin/perl -w
use strict;

use Getopt::Long "GetOptions";

my $LANGUAGE = shift @ARGV;
$LANGUAGE = 'target' unless ($LANGUAGE);

my $lno = 0;
while(my $line = <>) {
    $lno++;
    chomp $line;

    my @fields = split / \|\|\| /, $line;

    if ($LANGUAGE eq "source" or $LANGUAGE eq "both") {
        my @cwords = split /\s+/, $fields[0];
        foreach my $token (@cwords) {
            my @parts = split /_(?!.*_)/, $token;
            if (scalar @parts == 2) {
                $token = $parts[0]
            } else {
                print STDERR "WARNING: invalid tagged token $token\n";
            }
        }
        $fields[0] = join ' ', @cwords;
    }

    if ($LANGUAGE eq "target" or $LANGUAGE eq "both") {
        my @cwords = split /\s+/, $fields[1];
        foreach my $token (@cwords) {
            my @parts = split /_(?!.*_)/, $token;
            if (scalar @parts == 2) {
                $token = $parts[1]
            } else {
                print STDERR "WARNING: invalid tagged token $token\n";
            }
        }
        $fields[0] = join ' ', @cwords;
    }

    print join ' ||| ', @fields;
    print "\n";
}
