#!/usr/bin/perl -w
use strict;

die "Usage: $0 file1.txt file2.txt [file3.txt ...]\n\n  Performs a per-line concatenation of all files using the ||| seperator.\n\n" unless scalar @ARGV > 1;

my @fhs = ();
for my $file (@ARGV) {
  my $fh;
  if ($file =~ /\.gz$/) {
    open $fh, "gunzip -c $file|" or die "Can't fork gunzip -c $file: $!";
  } else {
    open $fh, "<$file" or die "Can't read $file: $!";
  }
  binmode($fh,":utf8");
  push @fhs, $fh;
}
binmode(STDOUT,":utf8");
binmode(STDERR,":utf8");

my $bad = 0;
my $lc = 0;
my $done = 0;
my $fl = 0;
while(1) {
  my @line;
  $lc++;
  if ($lc % 100000 == 0) { print STDERR " [$lc]\n"; $fl = 0; }
  elsif ($lc % 2500 == 0) { print STDERR "."; $fl = 1; }
  my $anum = 0;
  for my $fh (@fhs) {
    my $r = <$fh>;
    if (!defined $r) {
      die "Mismatched number of lines.\n" if scalar @line > 0;
      $done = 1;
      last;
    }
    $r =~ s/\r//g;
    chomp $r;
    if ($r =~ /\|\|\|/) {
      $r = '';
      $bad++;
    }
    warn "$ARGV[$anum]:$lc contains a ||| symbol - please remove.\n" if $r =~ /\|\|\|/;
    $r =~ s/\|\|\|/ /g;
    $r =~ s/\s+/ /g;
    $r =~ s/^ +//;
    $r =~ s/ +$//;
    $anum++;
    push @line, $r;
  }
  last if $done;
  print STDOUT join(' ||| ', @line) . "\n";
}
print STDERR "\n" if $fl;
for (my $i = 1; $i < scalar @fhs; $i++) {
  my $fh = $fhs[$i];
  my $r = <$fh>;
  die "Mismatched number of lines.\n" if defined $r;
}
print STDERR "Number of lines containing ||| was: $bad\n" if $bad > 0;

