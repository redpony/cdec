#!/usr/bin/perl -w
use strict;

my $PATCH = shift @ARGV;
my $TGT = 1;
my $APPEND;
while ($PATCH eq "-s" || $PATCH eq "-a") {
    if ($PATCH eq "-s") {
        undef $TGT;
    } else {
        $APPEND = 1;
    }
    $PATCH = shift @ARGV;
}

die "Usage: $0 [-s] [-a] tagged.en[_fr] < lexical.en_fr_al[_...]\n" unless $PATCH;

open P, "<$PATCH" or die "Can't read tagged corpus $PATCH: $!";
my $first=<P>; close P;
my @fields = split / \|\|\| /, $first;
die "Bad format!" if (scalar @fields > 2);

if (scalar @fields != 1) {
  # TODO support this
  die "Patching source and target not supported yet!";
}

my $line = 0;
open P, "<$PATCH" or die "Can't read tagged corpus $PATCH: $!";
while(my $pline = <P>) {
  chomp $pline;
  $line++;
  my $line = <>;
  die "Too few lines in lexical corpus!" unless $line;
  chomp $line;
  @fields = split / \|\|\| /, $line;
  my @pwords = split /\s+/, $pline;
  if ($TGT) {
      my @lwords = split /\s+/, $fields[1];
      die "Length mismatch in line $line!\n" unless (scalar @pwords == scalar @lwords);
      if ($APPEND) {
          foreach my $i (0..(scalar @pwords-1)) {
              $lwords[$i] = $lwords[$i] . '_' . $pwords[$i];
          }
          $fields[1] = join ' ', @lwords;
      } else {
          $fields[1] = $pline;
      }
  } else { # source side
      my @lwords = split /\s+/, $fields[0];
      die "Length mismatch in line $line!\n" unless (scalar @pwords == scalar @lwords);
      if ($APPEND) {
          foreach my $i (0..(scalar @pwords-1)) {
              $lwords[$i] = $lwords[$i] . '_' . $pwords[$i];
          }
          $fields[0] = join ' ', @lwords;
      } else {
          $fields[0] = $pline;
      }
  }
  print join ' ||| ', @fields;
  print "\n";
}


