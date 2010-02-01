#!/usr/bin/perl -w
use utf8;
use strict;

my $LIMIT_SIZE=30;

my ($effile, $model1, $imodel1, $orthof, $orthoe, $class_e, $class_f) = @ARGV;
die "Usage: $0 corpus.fr-en corpus.f-e.model1 corpus.e-f.model1 corpus.orthonorm-dict.f corpus.orthnorm-dict.e class.e class.f\n" unless $effile && -f $effile && $model1 && -f $model1 && $imodel1 && -f $imodel1 && $orthof && -f $orthof && $orthoe && -f $orthoe && -f $class_e && -f $class_f;

my %eclass = ();
my %fclass = ();
load_classes($class_e, \%eclass);
load_classes($class_f, \%fclass);

our %cache;
open EF, "<$effile" or die;
open M1, "<$model1" or die;
open IM1, "<$imodel1" or die;
binmode(EF,":utf8");
binmode(M1,":utf8");
binmode(IM1,":utf8");
binmode(STDOUT,":utf8");
my %model1;
print STDERR "Reading model1...\n";
my %sizes = ();
while(<M1>) {
  chomp;
  my ($f, $e, $lp) = split /\s+/;
  $model1{$f}->{$e} = 1;
  $sizes{$f}++;
}
close M1;

my $inv_add = 0;
my %invm1;
print STDERR "Reading inverse model1...\n";
my %esizes=();
while(<IM1>) {
  chomp;
  my ($e, $f, $lp) = split /\s+/;
  $invm1{$e}->{$f} = 1;
  $esizes{$e}++;
  if (($sizes{$f} or 0) < $LIMIT_SIZE && !(defined $model1{$f}->{$e})) {
    $model1{$f}->{$e} = 1;
    $sizes{$f}++;
    $inv_add++;
  }
}
close IM1;
print STDERR "Added $inv_add from inverse model1\n";

open M1, "<$model1" or die;
binmode(M1,":utf8");
my $dir_add = 0;
print STDERR "Reading model1 (again) for extra inverse translations...\n";
while(<M1>) {
  chomp;
  my ($f, $e, $lp) = split /\s+/;
  if (($esizes{$e} or 0) < $LIMIT_SIZE && !(defined $invm1{$e}->{$f})) {
    $invm1{$e}->{$f} = 1;
    $esizes{$e}++;
    $dir_add++;
  }
}
close M1;
print STDERR "Added $dir_add from model 1\n";
print STDERR "Generating grammars...\n";
open OE, "<$orthoe" or die;
binmode(OE,":utf8");
my %oe_dict;
while(<OE>) {
  chomp;
  my ($a, $b) = split / \|\|\| /, $_;
  die "BAD: $_" unless defined $a && defined $b;
  $oe_dict{$a} = $b;
}
close OE;
open OF, "<$orthof" or die;
binmode(OF,":utf8");
my %of_dict;
while(<OF>) {
  chomp;
  my ($a, $b) = split / \|\|\| /, $_;
  die "BAD: $_" unless defined $a && defined $b;
  $of_dict{$a} = $b;
}
close OF;
$of_dict{'<eps>'} = '<eps>';
$oe_dict{'<eps>'} = '<eps>';

my $MIN_FEATURE_COUNT = 0;
my $ADD_PREFIX_ID = 0;
my $ADD_CLASS_CLASS = 1;
my $ADD_LEN = 1;
my $ADD_SIM = 1;
my $ADD_DICE = 1;
my $ADD_111 = 1;
my $ADD_ID = 1;
my $ADD_PUNC = 1;
my $ADD_NULL = 0;
my $ADD_STEM_ID = 1;
my $ADD_SYM = 0;
my $BEAM_RATIO = 50;

my %fdict;
my %fcounts;
my %ecounts;

my %sdict;

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

my $specials = 0;
my $fc = 1000000;
my $sids = 1000000;
for my $f (sort keys %fdict) {
  my $re = $fdict{$f};
  my $max;
  for my $e (sort {$re->{$b} <=> $re->{$a}} keys %$re) {
    my $efcount = $re->{$e};
    unless (defined $max) { $max = $efcount; }
    my $m1 = $model1{$f}->{$e};
    my $im1 = $invm1{$e}->{$f};
    my $is_good_pair = (defined $m1);
    my $is_inv_good_pair = (defined $im1);
    my $dice = 2 * $efcount / ($ecounts{$e} + $fcounts{$f});
    my @feats;
    if ($efcount > $MIN_FEATURE_COUNT) {
      $fc++;
      push @feats, "F$fc=1";
    }
    if ($ADD_SYM && $is_good_pair && $is_inv_good_pair) { push @feats, 'Sym=1'; }
    my $oe = $oe_dict{$e};
    die "Can't find orthonorm form for $e" unless defined $oe;
    my $of = $of_dict{$f};
    die "Can't find orthonorm form for $f" unless defined $of;
    my $len_e = length($oe);
    my $len_f = length($of);
    push @feats, "Dice=$dice" if $ADD_DICE;
    if ($ADD_CLASS_CLASS) {
      my $ce = $eclass{$e} or die "E- no class for: $e";
      my $cf = $fclass{$f} or die "F- no class for: $f";
      push @feats, "C${cf}_${ce}=1";
    }
    my $is_null = undef;
    if ($ADD_NULL && $f eq '<eps>') {
      push @feats, "IsNull=1";
      $is_null = 1;
    }
    if ($ADD_LEN) {
      if (!$is_null) {
        my $dlen = abs($len_e - $len_f);
        push @feats, "DLen=$dlen";
      }
    }
    my $f_num = ($of =~ /^-?\d[0-9\.\,]+%?$/ && (length($of) > 3));
    my $e_num = ($oe =~ /^-?\d[0-9\.\,]+%?$/ && (length($oe) > 3));
    my $both_non_numeric = (!$e_num && !$f_num);
    if ($ADD_STEM_ID) {
      my $el = 4;
      my $fl = 4;
      if ($oe =~ /^al|re|co/) { $el++; }
      if ($of =~ /^al|re|co/) { $fl++; }
      if ($oe =~ /^trans|inter/) { $el+=2; }
      if ($of =~ /^trans|inter/) { $fl+=2; }
      if ($fl > length($of)) { $fl = length($of); }
      if ($el > length($oe)) { $el = length($oe); }
      my $sf = substr $of, 0, $fl;
      my $se = substr $oe, 0, $el;
      my $id = $sdict{$sf}->{$se};
      if (!$id) {
        $sids++;
	$sdict{$sf}->{$se} = $sids;
	$id = $sids;
      }
      push @feats, "S$id=1";
    }
    if ($ADD_PREFIX_ID) {
      if ($len_e > 3 && $len_f > 3 && $both_non_numeric) { 
        my $pe = substr $oe, 0, 3;
        my $pf = substr $of, 0, 3;
        if ($pe eq $pf) { push @feats, "PfxIdentical=1"; }
      }
    }
    if ($ADD_SIM) {
      my $ld = 0;
      my $eff = $len_e;
      if ($eff < $len_f) { $eff = $len_f; }
      if (!$is_null) {
        $ld = ($eff - levenshtein($oe, $of)) / sqrt($eff);
      }
      if ($ld > 1.5) { $is_good_pair = 1; }
      push @feats, "OrthoSim=$ld";
    }
    my $ident = ($e eq $f);
    if ($ident) { $is_good_pair = 1; }
    if ($ident && $ADD_ID) { push @feats, "Identical=$len_e"; }
    if ($efcount == 1 && $ecounts{$e} == 1 && $fcounts{$f} == 1) {
      $is_good_pair = 1;
      if ($ADD_111) {
        push @feats, "OneOneOne=1";
      }
    }
    if ($ADD_PUNC) {
      if ($f =~ /^[!,\-\/"':;=+?.()\[\]«»]+$/ && $e =~ /[a-z]+/) {
        push @feats, "PuncMiss=1";
      }
    }
    my $is_special = ($is_good_pair && !(defined $m1));
    $specials++ if $is_special;
    print STDERR "$f -> $e\n" if $is_special;
    print "1 ||| $f ||| $e ||| @feats\n" if $is_good_pair;
    print "2 ||| $e ||| $f ||| @feats\n" if $is_inv_good_pair;
  }
}
print STDERR "Added $specials special rules that were not in the M1 set\n";


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

sub load_classes {
  my ($file, $ref) = @_;
  print STDERR "Reading classes from $file...\n";
  open F, "<$file" or die "Can't read $file: $!";
  binmode(F, ":utf8") or die;
  while(<F>) {
    chomp;
    my ($word, $class) = split /\s+/;
#    print STDERR "'$word' -> $class\n";
    $ref->{$word} = $class;
  }
  close F;
}

