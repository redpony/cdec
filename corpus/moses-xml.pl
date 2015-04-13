#!/usr/bin/perl -w

use strict;
$|++;

my $msg = "Usage: $0 (escape|unescape)\n\n  Escapes XMl entities and other special characters for use with Moses.\n\n";

die $msg unless scalar @ARGV == 1;

if ($ARGV[0] eq "escape") {
    while (<STDIN>) {
        $_ =~ s/\&/\&amp;/g;   # escape escape
        $_ =~ s/\|/\&#124;/g;  # factor separator
        $_ =~ s/\</\&lt;/g;    # xml
        $_ =~ s/\>/\&gt;/g;    # xml
        $_ =~ s/\'/\&apos;/g;  # xml
        $_ =~ s/\"/\&quot;/g;  # xml
        $_ =~ s/\[/\&#91;/g;   # syntax non-terminal
        $_ =~ s/\]/\&#93;/g;   # syntax non-terminal
        print;
    }
} elsif ($ARGV[0] eq "unescape") {
    while (<STDIN>) {
        $_ =~ s/\&#124;/\|/g;  # factor separator
        $_ =~ s/\&lt;/\</g;    # xml
        $_ =~ s/\&gt;/\>/g;    # xml
        $_ =~ s/\&apos;/\'/g;  # xml
        $_ =~ s/\&quot;/\"/g;  # xml
        $_ =~ s/\&#91;/\[/g;   # syntax non-terminal
        $_ =~ s/\&#93;/\]/g;   # syntax non-terminal
        $_ =~ s/\&amp;/\&/g;   # escape escape
        print;
    }
} else {
    die $msg;
}
