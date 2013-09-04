#!/usr/bin/perl -w

use IO::Handle;
STDOUT->autoflush(1);

while (<>) {
  $output = "";
  @tokens = split;
  $lspace = 0;
  $qflag = 0;
  for ($i=0; $i<=$#tokens; $i++) {
    $token = $tokens[$i];
    $prev = $next = "";
    $rspace = 1;
    if ($i > 0) {
      $prev = $tokens[$i-1];
    }
    if ($i < $#tokens) {
      $next = $tokens[$i+1];
    }

    # possessives join to the left
    if ($token =~ /^(n't|'(s|m|re|ll|ve|d))$/) {
      $lspace = 0;
    } elsif ($token eq "'" && $prev =~ /s$/) {
      $lspace = 0;

    # hyphen only when a hyphen, not a dash
    } elsif ($token eq "-" && $prev =~ /[A-Za-z0-9]$/ && $next =~ /^[A-Za-z0-9]/) {
      $lspace = $rspace = 0;

    # quote marks alternate
    } elsif ($token eq '"') {
      if ($qflag) {
        $lspace = 0;
      } else {
        $rspace = 0;
      }
      $qflag = !$qflag;

    # period joins on both sides when a decimal point
    } elsif ($token eq "." && $prev =~ /\d$/ && $next =~ /\d$/) {
      $lspace = $rspace = 0;

    # Left joiners
    } elsif ($token =~ /^[.,:;?!%)\]]$/) {
      $lspace = 0;
    # Right joiners
    } elsif ($token =~ /^[$(\[]$/) {
      $rspace = 0;
    # Joiners on both sides
    } elsif ($token =~ /^[\/]$/) {
      $lspace = $rspace = 0;
    }

    if ($lspace) {
      $output .= " ";
    }
    $output .= $token;
    $lspace = $rspace;
  }
  print "$output\n";
}
