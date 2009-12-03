#!/usr/bin/perl -w
use utf8;
use strict;
my ($effile, $model1) = @ARGV;
die "Usage: $0 corpus.fr-en corpus.model1\n" unless $effile && -f $effile && $model1 && -f $model1;

open EF, "<$effile" or die;
open M1, "<$model1" or die;
binmode(EF,":utf8");
binmode(M1,":utf8");
binmode(STDOUT,":utf8");
my %model1;
while(<M1>) {
  chomp;
  my ($f, $e, $lp) = split /\s+/;
  $model1{$f}->{$e} = $lp;
}

my $ADD_MODEL1 = 0;      # found that model1 hurts performance
my $IS_FRENCH_F = 0;     # indicates that the f language is french
my $IS_ARABIC_F = 1;     # indicates that the f language is arabic
my $ADD_PREFIX_ID = 0;
my $ADD_LEN = 1;
my $ADD_LD = 0;
my $ADD_DICE = 1;
my $ADD_111 = 1;
my $ADD_ID = 1;
my $ADD_PUNC = 1;
my $ADD_NUM_MM = 1;
my $ADD_NULL = 1;
my $BEAM_RATIO = 50;

my %fdict;
my %fcounts;
my %ecounts;

while(<EF>) {
  chomp;
  my ($f, $e) = split /\s*\|\|\|\s*/;
  my @es = split /\s+/, $e;
  my @fs = split /\s+/, $f;
  for my $ew (@es){ $ecounts{$ew}++; }
  push @fs, '<eps>' if $ADD_NULL;
  for my $fw (@fs){ $fcounts{$fw}++; }
  for my $fw (@fs){
    for my $ew (@es){
      $fdict{$fw}->{$ew}++;
    }
  }
}

print STDERR "Dice 0\n" if $ADD_DICE;
print STDERR "OneOneOne 0\nId_OneOneOne 0\n" if $ADD_111;
print STDERR "Identical 0\n" if $ADD_ID;
print STDERR "PuncMiss 0\n" if $ADD_PUNC;
print STDERR "IsNull 0\n" if $ADD_NULL;
print STDERR "Model1 0\n" if $ADD_MODEL1;
print STDERR "DLen 0\n" if $ADD_LEN;
print STDERR "NumMM 0\n" if $ADD_NUM_MM;
print STDERR "Level 0\n" if $ADD_LD;
print STDERR "PfxIdentical 0\n" if ($ADD_PREFIX_ID);
my $fc = 1000000;
for my $f (sort keys %fdict) {
  my $re = $fdict{$f};
  my $max;
  for my $e (sort {$re->{$b} <=> $re->{$a}} keys %$re) {
    my $efcount = $re->{$e};
    unless (defined $max) { $max = $efcount; }
    my $m1 = $model1{$f}->{$e};
    unless (defined $m1) { next; }
    $fc++;
    my $dice = 2 * $efcount / ($ecounts{$e} + $fcounts{$f});
    my $feats = "F$fc=1";
    my $oe = $e;
    my $len_e = length($oe);
    my $of = $f;   # normalized form
    if ($IS_FRENCH_F) {
      # see http://en.wikipedia.org/wiki/Use_of_the_circumflex_in_French
      $of =~ s/â/as/g;
      $of =~ s/ê/es/g;
      $of =~ s/î/is/g;
      $of =~ s/ô/os/g;
      $of =~ s/û/us/g;
    } elsif ($IS_ARABIC_F) {
      if (length($of) > 1 && !($of =~ /\d/)) {
        $of =~ s/\$/sh/g;
      }
    }
    my $len_f = length($of);
    $feats .= " Model1=$m1" if ($ADD_MODEL1);
    $feats .= " Dice=$dice" if $ADD_DICE;
    my $is_null = undef;
    if ($ADD_NULL && $f eq '<eps>') {
      $feats .= " IsNull=1";
      $is_null = 1;
    }
    if ($ADD_LEN) {
      if (!$is_null) {
        my $dlen = abs($len_e - $len_f);
        $feats .= " DLen=$dlen";
      }
    }
    my $f_num = ($of =~ /^-?\d[0-9\.\,]+%?$/);  # this matches *two digit* and more numbers
    my $e_num = ($oe =~ /^-?\d[0-9\.\,]+%?$/);
    my $both_non_numeric = (!$e_num && !$f_num);
    if ($ADD_NUM_MM && (($f_num && !$e_num) || ($e_num && !$f_num))) {
      $feats .= " NumMM=1";
    }
    if ($ADD_PREFIX_ID) {
      if ($len_e > 3 && $len_f > 3 && $both_non_numeric) { 
        my $pe = substr $oe, 0, 3;
        my $pf = substr $of, 0, 3;
        if ($pe eq $pf) { $feats .= " PfxIdentical=1"; }
      }
    }
    if ($ADD_LD) {
      my $ld = 0;
      if ($is_null) { $ld = length($e); } else {
        $ld = levenshtein($e, $f);
      }
      $feats .= " Leven=$ld";
    }
    my $ident = ($e eq $f);
    if ($ident && $ADD_ID) { $feats .= " Identical=1"; }
    if ($ADD_111 && ($efcount == 1 && $ecounts{$e} == 1 && $fcounts{$f} == 1)) {
      if ($ident && $ADD_ID) {
        $feats .= " Id_OneOneOne=1";
      }
      $feats .= " OneOneOne=1";
    }
    if ($ADD_PUNC) {
      if (($f =~ /^[0-9!\$%,\-\/"':;=+?.()«»]+$/ && $e =~ /[a-z]+/) ||
          ($e =~ /^[0-9!\$%,\-\/"':;=+?.()«»]+$/ && $f =~ /[a-z]+/)) {
        $feats .= " PuncMiss=1";
      }
    }
    my $r = (0.5 - rand)/5;
    print STDERR "F$fc $r\n";
    print "$f ||| $e ||| $feats\n";
  }
}

sub levenshtein
{
    # $s1 and $s2 are the two strings
    # $len1 and $len2 are their respective lengths
    #
    my ($s1, $s2) = @_;
    my ($len1, $len2) = (length $s1, length $s2);

    # If one of the strings is empty, the distance is the length
    # of the other string
    #
    return $len2 if ($len1 == 0);
    return $len1 if ($len2 == 0);

    my %mat;

    # Init the distance matrix
    #
    # The first row to 0..$len1
    # The first column to 0..$len2
    # The rest to 0
    #
    # The first row and column are initialized so to denote distance
    # from the empty string
    #
    for (my $i = 0; $i <= $len1; ++$i)
    {
        for (my $j = 0; $j <= $len2; ++$j)
        {
            $mat{$i}{$j} = 0;
            $mat{0}{$j} = $j;
        }

        $mat{$i}{0} = $i;
    }

    # Some char-by-char processing is ahead, so prepare
    # array of chars from the strings
    #
    my @ar1 = split(//, $s1);
    my @ar2 = split(//, $s2);

    for (my $i = 1; $i <= $len1; ++$i)
    {
        for (my $j = 1; $j <= $len2; ++$j)
        {
            # Set the cost to 1 iff the ith char of $s1
            # equals the jth of $s2
            # 
            # Denotes a substitution cost. When the char are equal
            # there is no need to substitute, so the cost is 0
            #
            my $cost = ($ar1[$i-1] eq $ar2[$j-1]) ? 0 : 1;

            # Cell $mat{$i}{$j} equals the minimum of:
            #
            # - The cell immediately above plus 1
            # - The cell immediately to the left plus 1
            # - The cell diagonally above and to the left plus the cost
            #
            # We can either insert a new char, delete a char or
            # substitute an existing char (with an associated cost)
            #
            $mat{$i}{$j} = min([$mat{$i-1}{$j} + 1,
                                $mat{$i}{$j-1} + 1,
                                $mat{$i-1}{$j-1} + $cost]);
        }
    }

    # Finally, the Levenshtein distance equals the rightmost bottom cell
    # of the matrix
    #
    # Note that $mat{$x}{$y} denotes the distance between the substrings
    # 1..$x and 1..$y
    #
    return $mat{$len1}{$len2};
}


# minimal element of a list
#
sub min
{
    my @list = @{$_[0]};
    my $min = $list[0];

    foreach my $i (@list)
    {
        $min = $i if ($i < $min);
    }

    return $min;
}

