#!/usr/bin/perl -w
use strict;
use utf8;

##### EDIT THESE SETTINGS ####################################################
my $AUTOMATIC_INCLUDE_IF_SHORTER_THAN = 7; # if both are shorter, include
my $MAX_ZSCORE = 2.0; # how far from the mean can the (log)ratio be?
##############################################################################

die "Usage: $0 [-NNN] corpus.fr-en\n\n  Filter sentence pairs containing sentences longer than NNN words (where NNN\n  is 150 by default) or whose log length ratios are $MAX_ZSCORE stddevs away from the\n  mean log ratio.\n\n" unless scalar @ARGV == 1 || scalar @ARGV == 2;
binmode(STDOUT,":utf8");
binmode(STDERR,":utf8");

my $MAX_LENGTH = 150;  # discard a sentence if it is longer than this
if (scalar @ARGV == 2) {
  my $fp = shift @ARGV;
  die "Expected -NNN for first parameter, but got $fp\n" unless $fp =~ /^-(\d+)$/;
  $MAX_LENGTH=$1;
}

my $corpus = shift @ARGV;

die "Cannot read from STDIN\n" if $corpus eq '-';
my $ff = "<$corpus";
$ff = "gunzip -c $corpus|" if $ff =~ /\.gz$/;

print STDERR "Max line length (monolingual): $MAX_LENGTH\n";
print STDERR "              Parallel corpus: $corpus\n";

open F,$ff or die "Can't read $corpus: $!";
binmode(F,":utf8");

my $rat_max = log(9);
my $lrm = 0;
my $zerof = 0;
my $zeroe = 0;
my $bad_format = 0;
my $absbadrat = 0;
my $overlene = 0;
my $overlenf = 0;
my $lines = 0;
my @lograts = ();
while(<F>) {
  $lines++;
  if ($lines % 100000 == 0) { print STDERR " [$lines]\n"; }
  elsif ($lines % 2500 == 0) { print STDERR "."; }
  my ($sf, $se, @d) = split /\s*\|\|\|\s*| *\t */;
  if (scalar @d != 0 or !defined $se) {
    $bad_format++;
    if ($bad_format > 100 && ($bad_format / $lines) > 0.02) {
      die "$bad_format / $lines : Corpus appears to be incorretly formatted, example: $_";
    }
    next;
  }
  my @fs = ();
  my @es = ();
  if (defined $sf && length($sf) > 0) { @fs = split /\s+/, $sf; }
  if (defined $se && length($se) > 0) { @es = split /\s+/, $se; }
  my $flen = scalar @fs;
  my $elen = scalar @es;
  if ($flen == 0) {
    $zerof++;
    next;
  }
  if ($elen == 0) {
    $zeroe++;
    next;
  }
  if ($flen > $MAX_LENGTH) {
    $overlenf++;
    next;
  }
  if ($elen > $MAX_LENGTH) {
    $overlene++;
    next;
  }
  if ($elen >= $AUTOMATIC_INCLUDE_IF_SHORTER_THAN ||
      $flen >= $AUTOMATIC_INCLUDE_IF_SHORTER_THAN) {
    my $lograt = log($flen) - log($elen);
    if (abs($lograt) > $rat_max) {
      $absbadrat++;
      next;
    }
    $lrm += $lograt;
    push @lograts, $lograt;
  }
}
close F;

print STDERR "\nComputing statistics...\n";
my $lmean = $lrm / scalar @lograts;

my $lsd = 0;
for my $lr (@lograts) {
  $lsd += ($lr - $lmean)**2;
}
$lsd = sqrt($lsd / scalar @lograts);
@lograts = ();

my $pass1_discard = $zerof + $zeroe + $absbadrat + $overlene + $overlenf + $bad_format;
my $discard_rate = int(10000 * $pass1_discard / $lines) / 100;
print STDERR "      Total lines: $lines\n";
print STDERR " Already discared: $pass1_discard\t(discard rate = $discard_rate%)\n";
print STDERR "   Mean F:E ratio: " . exp($lmean) . "\n";
print STDERR " StdDev F:E ratio: " . exp($lsd) . "\n";
print STDERR "Writing...\n";
open F,$ff or die "Can't reread $corpus: $!";
binmode(F,":utf8");
my $to = 0;
my $zviol = 0;
my $worstz = -1;
my $worst = "\n";
$lines = 0;
while(<F>) {
  $lines++;
  if ($lines % 100000 == 0) { print STDERR " [$lines]\n"; }
  elsif ($lines % 2500 == 0) { print STDERR "."; }
  my ($sf, $se, @d) = split /\s*\|\|\|\s*| *\t */;
  if (!defined $se) { next; }
  my @fs = split /\s+/, $sf;
  my @es = split /\s+/, $se;
  my $flen = scalar @fs;
  my $elen = scalar @es;
  next if ($flen == 0);
  next if ($elen == 0);
  next if ($flen > $MAX_LENGTH);
  next if ($elen > $MAX_LENGTH);
  if ($elen >= $AUTOMATIC_INCLUDE_IF_SHORTER_THAN ||
      $flen >= $AUTOMATIC_INCLUDE_IF_SHORTER_THAN) {
    my $lograt = log($flen) - log($elen);
    if (abs($lograt) > $rat_max) {
      $absbadrat++;
      next;
    }
    my $zscore = abs($lograt - $lmean) / $lsd;
    if ($elen > $AUTOMATIC_INCLUDE_IF_SHORTER_THAN &&
        $flen > $AUTOMATIC_INCLUDE_IF_SHORTER_THAN && $zscore > $worstz) { $worstz = $zscore; $worst = $_; }
    if ($zscore > $MAX_ZSCORE) {
      $zviol++;
      next;
    }
    print;
  } else {
    print;
  }
  $to++;
}
my $discard_rate2 = int(10000 * $zviol / ($lines - $pass1_discard)) / 100;
print STDERR "\n    Lines printed: $to\n Ratio violations: $zviol\t(discard rate = $discard_rate2%)\n";
print STDERR "    Worst z-score: $worstz\n         sentence: $worst";
exit 0;

