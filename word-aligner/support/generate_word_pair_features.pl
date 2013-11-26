#!/usr/bin/perl -w
use utf8;
use strict;

my ($effile, $model1, $imodel1, $orthof, $orthoe, $class_e, $class_f, $sparse_m1, $use_prefixes, $use_suffixes) = @ARGV;
die "Usage: $0 corpus.fr-en corpus.f-e.full-model1 corpus.e-f.full-model1 corpus.orthonorm-dict.f corpus.orthnorm-dict.e class.e class.f corpus.f-e.model1\n" unless $effile && -f $effile && $model1 && -f $model1 && $imodel1 && -f $imodel1 && $orthof && -f $orthof && $orthoe && -f $orthoe && -f $class_e && -f $class_f && $sparse_m1 && -f $sparse_m1;

my %eclass = ();
my %fclass = ();
load_classes($class_e, \%eclass);
load_classes($class_f, \%fclass);

our @IDENT_BINS = qw (Ident0 Ident1 Ident2 Ident3 Ident4 Ident5 Ident6 Ident7 Ident8 Ident9);

my $MIN_MAGNITUDE = 0.001; # minimum value of a feature

our %cache;
open EF, "<$effile" or die;
open M1, "<$model1" or die;
open IM1, "<$imodel1" or die;
open SM1, "<$sparse_m1" or die;
binmode(EF,":utf8");
binmode(M1,":utf8");
binmode(IM1,":utf8");
binmode(SM1,":utf8");
binmode(STDOUT,":utf8");
my %model1;
print STDERR "Reading model1...\n";
my %sizes = ();
while(<M1>) {
  chomp;
  my ($f, $e, $lp) = split /\s+/;
  $model1{$f}->{$e} = sprintf("%.5g", 1e-12 + exp($lp));
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
  $invm1{$e}->{$f} = sprintf("%.5g", 1e-12 + exp($lp));
}
close IM1;

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

print STDERR "Reading sparse model 1 from $sparse_m1...\n";
my %s_m1;
while(<SM1>) {
  chomp;
  my ($f, $e, $lp) = split /\s+/;
  die unless defined $e && defined $f;
  $s_m1{$f}->{$e} = 1;
}
close SM1;

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
my $ADD_PREFIX_ID = 1;
my $ADD_LEN = 1;
my $ADD_SIM = 1;
my $ADD_DICE = 1;
my $ADD_111 = 1;
my $ADD_SPARSE_M1 = 0; # this is a very bad feature
my $SPARSE_111 = 1; # if 1-1-1, then don't include Model1 & Dice features
my $ADD_ID = 1;
my $ADD_PUNC = 1;
my $ADD_NULL = 1;
my $ADD_MODEL1 = 1;
my $ADD_NOMODEL1 = 0;
my $BEAM_RATIO = 50;
my $BIN_ORTHO = 1;
my $BIN_DLEN = 1;
my $BIN_IDENT = 1;
my $BIN_DICE = 1;

if ($ADD_NULL) { $fclass{'<eps>'}='NUL'; $eclass{'<eps>'} ='NUL'; }

my %fdict;
my %fcounts;
my %ecounts;

my %sdict;

while(<EF>) {
  chomp;
  my ($f, $e) = split /\s*\|\|\|\s*/;
  my @es = split /\s+/, $e;
  my @fs = split /\s+/, $f;
  for my $ew (@es){
    die "E: Empty word" if $ew eq '';
    $ecounts{$ew}++;
  }
  push @fs, '<eps>' if $ADD_NULL;
  my $i = 0;
  for my $fw (@fs){
    $i++;
    die "F: Empty word\nI=$i FS: @fs" if $fw eq '';
    $fcounts{$fw}++;
  }
  for my $fw (@fs){
    for my $ew (@es){
      $fdict{$fw}->{$ew}++;
    }
  }
}

print STDERR "Extracting word pair features...\n";
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
    my $is_null = undef;
    if ($f eq '<eps>') {
      $is_null = 1;
      $im1 = 0;  # probability not calcuated
    }
    die "No Model1 probability for $e | $f !" unless defined $m1;
    die "No inverse Model1 probability for $f | $e !" unless defined $im1;
    my $ident = ($e eq $f);
    my $total_eandf = $ecounts{$e} + $fcounts{$f};
    my $dice = 2 * $efcount / $total_eandf;
    my @feats;
    my $is_111 = ($efcount == 1 && $ecounts{$e} == 1 && $fcounts{$f} == 1);
    if ($is_111 && $ADD_111) {
      push @feats, "OneOneOne=1";
    }
    unless ($is_111 && $SPARSE_111) {
      if ($ADD_SPARSE_M1 && defined $s_m1{$f}->{$e}) {
        push @feats, "HighM1=1";
      }
      if (defined $m1 && $ADD_MODEL1) {
        if ($m1 > $MIN_MAGNITUDE) {
          push @feats, "Model1=$m1";
          my $m1d = sprintf("%.5g", sqrt($m1 * $dice));
          push @feats, "M1Dice=$m1d" if $m1d > $MIN_MAGNITUDE;
        } elsif ($ADD_NOMODEL1) {
          push @feats, 'NoModel1=1';
        }
        if ($im1 > $MIN_MAGNITUDE) {
          push @feats, "InvModel1=$im1" if $im1;
        } elsif ($ADD_NOMODEL1) {
          push @feats, 'NoInvModel1=1';
        }
        my $am1 = sprintf("%.5g", sqrt($m1 * $im1));
        push @feats, "AgrModel1=$am1" if $am1 > $MIN_MAGNITUDE;
      }
      if ($ADD_DICE) {
        if ($BIN_DICE) {
          push @feats, dicebin($dice) . '=1';
        } else {
          push @feats, "Dice=$dice";
        }
      }
    }
    my $oe = $oe_dict{$e};
    die "Can't find orthonorm form for $e" unless defined $oe;
    my $of = $of_dict{$f};
    die "Can't find orthonorm form for $f" unless defined $of;
    my $len_e = length($oe);
    my $len_f = length($of);
    if ($ADD_LEN) {
      if (!$is_null) {
        my $dlen = abs($len_e - $len_f);
        if ($BIN_DLEN) {
          push @feats, dlenbin($dlen) . '=1';
        } else {
          push @feats, "DLen=$dlen";
        }
      }
    }
    my $f_num = ($of =~ /^-?\d[0-9\.\,]+%?$/ && (length($of) > 2));
    my $e_num = ($oe =~ /^-?\d[0-9\.\,]+%?$/ && (length($oe) > 2));
    my $both_non_numeric = (!$e_num && !$f_num);

    unless ($total_eandf > 20) {
      if ($f_num && $e_num) {
        my $xf = $of;
        $xf =~ s/[.,\N{U+0087}]//g;
        my $xe = $oe;
        $xe =~ s/[.,\N{U+0087}]//g;
        if (($of ne $oe) && ($xe eq $xf)) { push @feats, "NumNearIdent=1"; }
      }
    }

    if ($ADD_SIM) {
      my $ld = 0;
      my $eff = $len_e;
      if ($eff < $len_f) { $eff = $len_f; }
      if (!$is_null) {
        $ld = ($eff - levenshtein($oe, $of)) / sqrt($eff);
      }
      if ($BIN_ORTHO) {
        push @feats, orthobin($ld) . '=1';
      } else {
        push @feats, "OrthoSim=$ld";
      }
    }
    my $f_is_punc = ($f =~ /^[!,\-\/"'`:;&=+?.()\[\]«»]+$/);
    if ($ident && $ADD_ID) {
      if ($f_is_punc) { push @feats, "IdentPunc=1"; }
      else {
        if ($e =~ /\d/ && $len_e > 2) { push @feats, "IdentNumber=1"; }
        if ($total_eandf < 8) { push @feats, "IdentRare=1"; }
        if ($BIN_IDENT) {
          push @feats, identbin($len_e) . '=1';
        } else {
          push @feats, "Identical=$len_e";
        }
      }
    }
    if ($ADD_PREFIX_ID && !$ident) {
      if ($len_e > 3 && $len_f > 3 && $both_non_numeric) { 
        my $pe = substr $oe, 0, 3;
        my $pf = substr $of, 0, 3;
        if ($pe eq $pf) { push @feats, "PfxIdentical=1"; }
      }
    }
    if ($ADD_PUNC) {
      if ($f_is_punc && $e =~ /[a-z]+/) {
        push @feats, "PuncMiss=1";
      }
    }
    if ($use_prefixes) {
      my $prefix1 = prefix_to_type($f, $e, 1);
      if (length $prefix1 > 0 && !$is_null) { push @feats, $prefix1."=1";}
      my $prefix2 = prefix_to_type($f, $e, 2);
      if (length $prefix2 > 0 && !$is_null) { push @feats, $prefix2."=1";}
      my $prefix3 = prefix_to_type($f, $e, 3);
      if (length $prefix3 > 0 && !$is_null) { push @feats, $prefix3."=1";}
      my $prefix1_reverse = prefix_to_type($e, $f, 1);
      if (length $prefix1_reverse > 0 && !$is_null) { push @feats, $prefix1_reverse."=1";}
      my $prefix2_reverse = prefix_to_type($e, $f, 2);
      if (length $prefix2_reverse > 0 && !$is_null) { push @feats, $prefix2_reverse."=1";}
      my $prefix3_reverse = prefix_to_type($e, $f, 3);
      if (length $prefix3_reverse > 0 && !$is_null) { push @feats, $prefix3_reverse."=1";}
    }
    if ($use_suffixes) {
      my $suffix1 = suffix_to_type($f, $e, 1);
      if (length $suffix1 > 0 && !$is_null) { push @feats, $suffix1."=1";}
      my $suffix2 = suffix_to_type($f, $e, 2);
      if (length $suffix2 > 0 && !$is_null) { push @feats, $suffix2."=1";}
      my $suffix3 = suffix_to_type($f, $e, 3);
      if (length $suffix3 > 0 && !$is_null) { push @feats, $suffix3."=1";}
      my $suffix1_reverse = suffix_to_type($e, $f, 1);
      if (length $suffix1_reverse > 0 && !$is_null) { push @feats, $suffix1_reverse."=1";}
      my $suffix2_reverse = suffix_to_type($e, $f, 2);
      if (length $suffix2_reverse > 0 && !$is_null) { push @feats, $suffix2_reverse."=1";}
      my $suffix3_reverse = suffix_to_type($e, $f, 3);
      if (length $suffix3_reverse > 0 && !$is_null) { push @feats, $suffix3_reverse."=1";}
    }
    print "$f ||| $e ||| @feats\n";
  }
}

# returns a feature string instantiating the pattern "(source_prefix,target)"
sub prefix_to_type
{
    # $f => src token
    # $e => tgt token
    my ($f, $e, $len_prefix) = @_;
    
    if (length $f > $len_prefix && index($e.$f, '=') < 0)
    {
        return substr($f, 0, $len_prefix)."-".$e;
    } 
    else
    {
        return "";
    }
}

# returns a feature string instantiating the pattern "(source_prefix,target)"
sub suffix_to_type
{
    # $f => src token
    # $e => tgt token
    my ($f, $e, $len_prefix) = @_;

    if ( (length $f) > $len_prefix && index($e.$f, '=') < 0) 
    {
        return substr($f, (length $f)-$len_prefix, $len_prefix)."_".$e;
    } 
    else 
    {
        return "";
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

sub dicebin {
  my $x = shift;
  if ($x < 0.05) { return 'DiceLT005'; }
  elsif ($x >= 0.05 && $x < 0.1) { return 'Dice005_01'; }
  elsif ($x >= 0.1 && $x < 0.2) { return 'Dice01_02'; }
  elsif ($x >= 0.2 && $x < 0.3) { return 'Dice02_03'; }
  elsif ($x >= 0.3 && $x < 0.4) { return 'Dice03_04'; }
  elsif ($x >= 0.4 && $x < 0.5) { return 'Dice04_05'; }
  elsif ($x >= 0.5 && $x < 0.6) { return 'Dice05_06'; }
  elsif ($x >= 0.6 && $x < 0.7) { return 'Dice06_07'; }
  elsif ($x >= 0.7 && $x < 0.8) { return 'Dice07_08'; }
  elsif ($x >= 0.8 && $x < 0.9) { return 'Dice08_09'; }
  elsif ($x >= 0.9 && $x < 1.0) { return 'Dice09_10'; }
  elsif ($x >= 1.0 && $x < 1.1) { return 'Dice10_11'; }
  elsif ($x >= 1.1 && $x < 1.2) { return 'Dice11_12'; }
  elsif ($x >= 1.2 && $x < 1.4) { return 'Dice12_14'; }
  elsif ($x >= 1.4 && $x < 1.6) { return 'Dice14_16'; }
  elsif ($x >= 1.6 && $x < 1.8) { return 'Dice16_18'; }
  elsif ($x >= 1.8 && $x < 2.0) { return 'Dice18_20'; }
  elsif ($x >= 2.0 && $x < 2.3) { return 'Dice20_23'; }
  elsif ($x >= 2.3) { return 'DiceGT23'; }
}

sub orthobin {
  my $x = shift;
  if ($x < 0.9) { return 'OrthoLT09'; }
  elsif ($x >= 0.9 && $x < 1.1) { return 'Ortho09_11'; }
  elsif ($x >= 1.1 && $x < 1.3) { return 'Ortho11_13'; }
  elsif ($x >= 1.3 && $x < 1.5) { return 'Ortho13_15'; }
  elsif ($x >= 1.5 && $x < 1.7) { return 'Ortho15_17'; }
  elsif ($x >= 1.7 && $x < 1.9) { return 'Ortho17_19'; }
  elsif ($x >= 1.9 && $x < 2.1) { return 'Ortho19_21'; }
  elsif ($x >= 2.1 && $x < 2.3) { return 'Ortho21_23'; }
  elsif ($x >= 2.3 && $x < 2.5) { return 'Ortho23_25'; }
  elsif ($x >= 2.5 && $x < 2.7) { return 'Ortho25_27'; }
  elsif ($x >= 2.7 && $x < 2.9) { return 'Ortho27_29'; }
  elsif ($x >= 2.9) { return 'OrthoGT29'; }
}

sub dlenbin {
  my $x = shift;
  if ($x == 0) { return 'DLen0'; }
  elsif ($x == 1) { return 'DLen1'; }
  elsif ($x == 2) { return 'DLen2'; }
  elsif ($x == 3) { return 'DLen3'; }
  elsif ($x == 4) { return 'DLen4'; }
  elsif ($x == 5) { return 'DLen5'; }
  elsif ($x == 6) { return 'DLen6'; }
  elsif ($x == 7) { return 'DLen7'; }
  elsif ($x == 8) { return 'DLen8'; }
  elsif ($x == 9) { return 'DLen9'; }
  elsif ($x >= 10) { return 'DLenGT10'; }
}

sub identbin {
  my $x = shift;
  if ($x == 0) { die; }
  $x = int(log($x + 1) / log(1.3));
  if ($x >= scalar @IDENT_BINS) { return $IDENT_BINS[-1]; }
  return $IDENT_BINS[$x];
}


