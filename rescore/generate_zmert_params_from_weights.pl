#!/usr/bin/perl -w

my %defaults;
$defaults{'LanguageModel'} = "Opt\t0\t10\t0\t2.5";
$defaults{'EgivenF'} = "Opt\t-5\t0.5\t-3\t0.5";
$defaults{'LexEGivenF'} = "Opt\t-5\t0.5\t-3\t0.5";
$defaults{'LexFGivenE'} = "Opt\t-5\t0.5\t-3\t0.5";
$defaults{'PassThrough'} = "Opt\t-Inf\t+Inf\t-10\t0";
$defaults{'WordPenalty'} = "Opt\t-Inf\t2\t-5\t0";
my $DEFAULT = "Opt\t-Inf\t+Inf\t-1\t+1";

while(<>) {
  next if /^#/;
  chomp;
  next if /^\s*$/;
  s/^\s+//;
  s/\s+$//;
  my ($a,$b) = split /\s+/;
  next unless ($a && $b);
  my $line = $DEFAULT;
  if ($defaults{$a}) { $line = $defaults{$a}; }
  print "$a\t|||\t$b\t$line\n";
}

print "normalization = none\n";

