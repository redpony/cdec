#!/usr/bin/perl -w

use strict;
my $script_dir; BEGIN { use Cwd qw/ abs_path /; use File::Basename; $script_dir = dirname(abs_path($0)); push @INC, $script_dir; }
use Getopt::Long;
use IPC::Open2;

my $CDEC = "$script_dir/../decoder/cdec";
my $LANG = 'de';

my $BEAM = 2.1;
my $OUTPUT = 'plf';
my $HELP;
my $VERBOSE;
my $PRESERVE_CASE;

GetOptions("decoder=s" => \$CDEC,
           "language=s" => \$LANG,
           "beam=f" => \$BEAM,
           "output=s" => \$OUTPUT,
           "verbose" => \$VERBOSE,
           "preserve_case" => \$PRESERVE_CASE,
           "help" => \$HELP
          ) or usage();

usage() if $HELP;

chdir $script_dir;

if ($VERBOSE) { $VERBOSE = ""; } else { $VERBOSE = " 2> /dev/null"; }
$LANG = lc $LANG;
die "Can't find $CDEC\n" unless -f $CDEC;
die "Can't execute $CDEC\n" unless -x $CDEC;
die "Don't know about language: $LANG\n" unless -d "./$LANG";
my $CONFIG="cdec-$LANG.ini";
die "Can't find $CONFIG" unless -f $CONFIG;
die "--output must be '1best' or 'plf'\n" unless ($OUTPUT =~ /^(plf|1best)$/);
print STDERR "(Run with --help for options)\n";
print STDERR "LANGUAGE: $LANG\n";
print STDERR "  OUTPUT: $OUTPUT\n";

my $CMD = "$CDEC -c $CONFIG";
my $IS_PLF;
if ($OUTPUT eq 'plf') {
  $IS_PLF = 1;
  $CMD .= " --csplit_preserve_full_word --csplit_output_plf --beam_prune $BEAM";
}
$CMD .= $VERBOSE;

print STDERR "Executing: $CMD\n";

open2(\*OUT, \*IN, $CMD) or die "Couldn't fork: $!";
binmode(STDIN,":utf8");
binmode(STDOUT,":utf8");
binmode(IN,":utf8");
binmode(OUT,":utf8");

while(<STDIN>) {
  chomp;
  s/^\s+//;
  s/\s+$//;
  my @words = split /\s+/;
  my @res = ();
  my @todo = ();
  my @casings = ();
  for (my $i=0; $i < scalar @words; $i++) {
    my $word = lc $words[$i];
    if (length($word)<6 || $word =~ /^[,\-0-9\.]+$/ || $word =~ /[@.\-\/:]/) {
      push @casings, 0;
      if ($IS_PLF) {
        push @res, "(('" . escape($word) . "',0,1),),";
      } else {
        if ($PRESERVE_CASE) {
          push @res, $words[$i];
        } else {
          push @res, $word;
        }
      }
    } else {
      push @casings, guess_casing($words[$i]);
      push @res, undef;
      push @todo, $word;
    }
  }
  if (scalar @todo > 0) {
    # print STDERR "TODO: @todo\n";
    my $tasks = join "\n", @todo;
    print IN "$tasks\n";
    for (my $i = 0; $i < scalar @res; $i++) {
      if (!defined $res[$i]) {
        my $seg = <OUT>;
        chomp $seg;
        unless ($IS_PLF) {
          $seg =~ s/^# //o;
        }
        if ($PRESERVE_CASE && $casings[$i]) { $seg = recase_words($seg); }
        $res[$i] = $seg;
      }
    }
  }
  if ($IS_PLF) {
    print '(';
    print join '', @res;
    print ")\n";
  } else {
    print "@res\n";
  }
}

close IN;
close OUT;

sub recase_words {
  my $word = shift;
  $word =~ s/\b(\w)/\u$1/g;
  return $word;
}

sub escape {
  $_ = shift;
  s/\\/\\\\/g;
  s/'/\\'/g;
  return $_;
}

sub guess_casing {
  my $word = shift @_;
  if (lc($word) eq $word) { return 0; } else { return 1; }
}

sub usage {
  print <<EOT;

Usage: $0 [OPTIONS] < file.txt

  Options:
    --decoder PATH        Path to cdec
    --lanugage XX         Two letter language code (de, ...)
    --beam NUM            Beam threshold, used with PLF output
                            (probably between 1.5 and 5.0)
    --output plf|1best    Output format, 1best or plf (lattice)
    --preserve_case       Preserve the casing of the input word
                            (model will be scored lowercase)
    --verbose             Show verbose decoder output

EOT
  exit(1);
}
