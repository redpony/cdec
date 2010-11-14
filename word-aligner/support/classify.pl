#!/usr/bin/perl -w
use strict;

die "Usage: $0 [-unk] classes.txt corpus.txt\n Applies a vocabulary map to a corpus\n" unless scalar @ARGV == 2 || (scalar @ARGV == 3 && $ARGV[0] eq '-unk');

my $unk = $ARGV[0] eq '-unk';
shift @ARGV if $unk;

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

my @cats;
while(<T>) {
  chomp;
  if ($unk) {
    @cats = map { $dict{$_} or "UNK" } split /\s+/;
  } else {
    @cats = map { $dict{$_} or die "Undefined class for $_"; } split /\s+/;
  }
  print "@cats\n";
}

