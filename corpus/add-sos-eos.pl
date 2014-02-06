#!/usr/bin/perl -w
use strict;

die "Usage: $0 corpus.fr[-en1-en2-...] [corpus.al out-corpus.al]\n" unless (scalar @ARGV == 1 || scalar @ARGV == 3);
my $filec = shift @ARGV;
my $filea = shift @ARGV;
my $ofilea = shift @ARGV;
open C, "<$filec" or die "Can't read $filec: $!";
if ($filea) {
  open A, "<$filea" or die "Can't read $filea: $!";
  open OA, ">$ofilea" or die "Can't write $ofilea: $!";
}
binmode(C, ":utf8");
binmode(STDOUT, ":utf8");
print STDERR "Adding <s> and </s> markers to input...\n";
print STDERR "    Reading corpus: $filec\n";
print STDERR "    Writing corpus: STDOUT\n";
print STDERR "Reading alignments: $filea\n" if $filea;
print STDERR "Writing alignments: $ofilea\n" if $filea;

my $lines = 0;
while(<C>) {
  $lines++;
  die "ERROR. Input line $filec:$lines should not contain SGML markup" if /<seg /;
  if ($lines % 100000 == 0) { print STDERR " [$lines]\n"; }
  elsif ($lines % 2500 == 0) { print STDERR "."; }
  chomp;
  my @fields = split / \|\|\| /;
  my $o = '';
  for my $field (@fields) {
    $o .= " ||| <s> $field </s>";
  }
  $o =~ s/^ \|\|\| //;
  if ($filea) {
    my $aa = <A>;
    die "ERROR. Mismatched number of lines between $filec and $filea\n" unless $aa;
    chomp $aa;
    my ($ff, $ee) = @fields;
    die "ERROR in $filec:$lines: expected 'source ||| target'" unless defined $ee;
    my @fs = split /\s+/, $ff;
    my @es = split /\s+/, $ee;
    my @as = split /\s+/, $aa;
    my @oas = ();
    push @oas, '0-0';
    my $flen = scalar @fs;
    my $elen = scalar @es;
    for my $ap (@as) {
      my ($a, $b) = split /-/, $ap;
      die "ERROR. Bad format in: @as" unless defined $a && defined $b;
      push @oas, ($a + 1) . '-' . ($b + 1);
    }
    push @oas, ($flen + 1) . '-' . ($elen + 1);
    print OA "@oas\n";
  }
  print "$o\n";
}
if ($filea) {
  close OA;
  my $aa = <A>;
  die "ERROR. Alignment input file $filea contains more lines than corpus file!\n" if $aa;
}
print STDERR "\nSUCCESS. Processed $lines lines.\n";

