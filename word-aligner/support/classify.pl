#!/usr/bin/perl -w
use strict;

die "Usage: $0 classes.txt corpus.txt" unless scalar @ARGV == 2;

my ($class, $text) = @ARGV;
open C, "<$class" or die "Can't read $class: $!";
open T, "<$text" or die "Can't read $text: $!";

my %dict = ();
my $cc = 0;
while(<C>) {
  chomp;
  my ($word, $cat) = split /\s+/;
  die "'$word' '$cat'" unless (defined $word && defined $cat);
  $dict{$word} = $cat;
  $cc++;
}
close C;
print STDERR "Loaded classes for $cc words\n";

while(<T>) {
  chomp;
  my @cats = map { $dict{$_} or die "Undefined class for $_"; } split /\s+/;
  print "@cats\n";
}

