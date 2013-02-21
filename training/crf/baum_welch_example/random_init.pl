#!/usr/bin/perl -w
while(<>) {
  chomp;
  my ($a,$b,@d) =split /\s+/;
  die "Bad input" if scalar @d > 0;
  $r = -rand() * rand() - 0.5;
  $r = 0 if $a =~ /^Uni:/;
  print "$a $r\n";
}
