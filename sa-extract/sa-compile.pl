#!/usr/bin/env perl

use strict;
use Getopt::Long;

my $cwd; BEGIN { use Cwd qw/ abs_path cwd /; use File::Basename; $cwd = cwd(); }

my $rootdir = `dirname $0`; chomp $rootdir;
my $compile = "$rootdir/compile_bin.py";
my $lcp = "$rootdir/lcp_ops.py";
die "Can't find $compile" unless -f $compile;
die "Can't execute $compile" unless -x $compile;

sub print_help;
sub cleanup;

my $alignment;
my $bitext;
my $catalog;
my $dryrun = 0;
my $group;
my $help = 0;
my $ini = "$rootdir/extract.ini";
my $lm;
my $precomp;
my $no_ini = 0;
my $remove;
my $type;
my $local_only = 1;
my $output;

# Process command-line options
if (GetOptions(
  "alignment=s" => \$alignment,
  "bitext=s" => \$bitext,
  "help" => \$help,
  "ini=s" => \$ini,
  "output=s" => \$output,
  "precomp-options=s" => \$precomp,
  "no-ini" => \$no_ini,
) == 0 || $help == 1 || @ARGV > 0){
  print_help;
  die "\n";
}

open(INI, $ini) or die "Can't read $ini: $!";

$bitext || die "You must specify a bitext with -b\n";
$alignment || die "You must specify an alignment with -a\n";

my $top_dir;
if (defined $output) {
  $top_dir = $output;
} else {
  $top_dir = "$cwd/sa-compiled";
}

my $type_dir = "$top_dir";

my $bitext_name;
my $bitext_f_file;
my $bitext_e_file;
my $bitext_dir;
if ($bitext){
  if ($bitext =~ /(.*)=(.*),(.*)/){
    $bitext_name = $1;
    $bitext_f_file = $2;
    $bitext_e_file = $3;
    -e $bitext_f_file || die "Could not find file $bitext_f_file\n";
    -e $bitext_e_file || die "Could not find file $bitext_e_file\n";    
  } else {
    $bitext_name = $bitext;
  }

  $bitext_dir = "$type_dir/bitext/$bitext_name";
  if ($bitext_f_file){
    if (-e $bitext_dir) {
      die "Bitext $bitext_name already exists\n";
    }
  } else {
    unless (-e $bitext_dir){
      die "No bitext $bitext_name. You must specify bitext files with -b\n";
    }
  }
}

my $max_nt = 2;
my $max_len = 5;
my $max_size = 15;
my $min_gap = 1;
my $rank1 = 100;
my $rank2 = 10;
my $precomp_file;
if ($precomp){
  unless ($bitext_name){
    die "You must specify a bitext with -b if using -p\n";
  }
  my @precomp_args = split(/,/, $precomp);
  my $precomp_arg;
  for $precomp_arg (@precomp_args){
    if ($precomp_arg =~ /(.*)=(.*)/){
      my $key = $1;
      my $value = $2;
      unless ($value =~ /^\d+$/){
        die "Value for -p option must be a positive integer, found $value\n";
      }
      if ($key eq "max-len"){ $max_len = $value; }
      elsif ($key eq "max-nt"){ $max_nt = $value; }
      elsif ($key eq "max-size"){ $max_size = $value; }
      elsif ($key eq "min-gap"){ $min_gap = $value; }
      elsif ($key eq "rank1"){ $rank1 = $value; }
      elsif ($key eq "rank2"){ $rank2 = $value; }
      else{
        die "Unknown option $key given for -p\n";
      }
    } else {
      die "When using -p, you must specify key-value pairs using syntax: <key1>=<value1>,...,<keyN>=<valueN>\n";
    }
  }
}
my $precomp_compile_needed = 0;
if ($bitext_name){
  $precomp_file = "$bitext_dir/precomp.$max_len.$max_nt.$max_size.$min_gap.$rank1.$rank2.bin";
  unless (-e $precomp_file){
    $precomp_compile_needed = 1;
  }
}

my $alignment_name;
my $alignment_file;
my $alignment_dir;
if ($alignment){
  $bitext || die "Specified alignment $alignment without specifying bitext using -b\n";
  if ($alignment =~ /(.*)=(.*)/){
    $alignment_name = $1;
    $alignment_file = $2;
    -e $alignment_file || die "Could not find file $alignment_file\n";
  } else {
    $alignment_name = $alignment;
  }

  $alignment_dir = "$bitext_dir/a/$alignment_name";
  if ($alignment_file){
    if (-e $alignment_dir){
      die "Alignment $alignment_name already exists for bitext $bitext_name\n";
    }
  } else {
    require_top_dirs();
    unless (-e $alignment_dir){
      die "No alignment $alignment_name for bitext $bitext_name\n";
    }
  }
}

if ($bitext_name){
  print STDERR " from files $bitext_f_file and $bitext_e_file\n";
} else {
  print " No bitext\n";
}
if ($precomp_compile_needed){
  print STDERR "   Precompilation needed: max-len=$max_len, max-nt=$max_nt, max-size=$max_size, min-gap=$min_gap, rank1=$rank1, rank2=$rank2\n";
}
if ($alignment_name){
  print STDERR " Alignment = $alignment_name";
  if ($alignment_file){
    print STDERR " from file $alignment_file\n";
  }
} else {
  print STDERR " No alignment\n";
}

my $script;
my $compile_dir;
$SIG{INT} = "cleanup";
$SIG{TERM} = "cleanup"; 
$SIG{HUP} = "cleanup";

  if ($bitext_e_file || $precomp_compile_needed || $alignment_file){
    my $compiled_e_file;
    my $compiled_f_file;

    $compile_dir = $top_dir;
    my $compile_top_dir = "$compile_dir";

    my $compile_bitext_dir = "$compile_top_dir/bitext/$bitext_name";
    if ($bitext_e_file){
      `mkdir -p $compile_bitext_dir`;
      print STDERR "\nCompiling bitext (f side)...\n";
      `$compile -s $bitext_f_file $compile_bitext_dir/f.sa.bin`;
      die "Command failed: $!" unless $? == 0;
      print STDERR "\nCompiling bitext (e side)...\n";
      `$compile -d $bitext_e_file $compile_bitext_dir/e.bin`;
      die "Command failed: $!" unless $? == 0;

      $compiled_f_file = "$compile_bitext_dir/f.sa.bin";
      $compiled_e_file = "$compile_bitext_dir/e.bin";
    } else { # bitext already compiled
      $compiled_f_file = "$bitext_dir/f.sa.bin";
      $compiled_e_file = "$bitext_dir/e.bin";
    }

    if ($precomp_compile_needed){
      `mkdir -p $compile_bitext_dir`;
      my $top_stats_file = "$compile_bitext_dir/f.top.$rank1";
      my $compiled_precomp_file = "$compile_bitext_dir/precomp.$max_len.$max_nt.$max_size.$min_gap.$rank1.$rank2.bin";
      my $cmd = "$lcp -t 4 $compiled_f_file | sort -nr | head -$rank1 > $top_stats_file";
      print STDERR "$cmd\n";
      `$cmd`;
      die "Command failed: $cmd" unless $? == 0;
      `$compile -r max-len=$max_len max-nt=$max_nt max-size=$max_size min-gap=$min_gap rank1=$rank1 rank2=$rank2 sa=$compiled_f_file $top_stats_file $compiled_precomp_file`;
      die "Command failed: $!" unless $? == 0;
    }

    if ($alignment_file){
      my $compile_alignment_dir = "$compile_top_dir/bitext/$bitext_name/a/$alignment_name";
      `mkdir -p $compile_alignment_dir`;
      print STDERR "\nCompiling alignment...\n";
      my $cmd= "$compile -a $alignment_file $compile_alignment_dir/a.bin";
      print STDERR "  $cmd\n";
      `$cmd`;
      die "Command failed: $!" unless $? == 0;

      print STDERR "\nCompiling lexical weights file...\n";
      $cmd="$compile -x $compiled_f_file $compiled_e_file $compile_alignment_dir/a.bin $compile_alignment_dir/lex.bin";
      print STDERR "  $cmd\n";
      `$cmd`;
      die "Command failed: $!" unless $? == 0;
    }

    chdir $compile_dir;
    print STDERR "Compiling done: $compile_dir\n";
  }
  
  unless ($no_ini){
    my $line;
    while($line=<INI>){
      $line =~ s/^([^#]*a_file\s*=\s*")(.*)("\s*)$/$1$alignment_dir\/a.bin$3/;
      $line =~ s/^([^#]*lex_file\s*=\s*")(.*)("\s*)$/$1$alignment_dir\/lex.bin$3/;
      $line =~ s/^([^#]*f_sa_file\s*=\s*")(.*)("\s*)$/$1$bitext_dir\/f.sa.bin$3/;
      $line =~ s/^([^#]*e_file\s*=\s*")(.*)("\s*)$/$1$bitext_dir\/e.bin$3/;
      $line =~ s/^([^#]*precompute_file\s*=\s*")(.*)("\s*)$/$1$bitext_dir\/precomp.$max_len.$max_nt.$max_size.$min_gap.$rank1.$rank2.bin$3/;

      $line =~ s/^([^#]*max_len\s*=\s*)(.*)(\s*)$/$1$max_len$3/;
      $line =~ s/^([^#]*max_nt\s*=\s*)(.*)(\s*)$/$1$max_nt$3/;
      $line =~ s/^([^#]*max_size\s*=\s*)(.*)(\s*)$/$1$max_size$3/;
      $line =~ s/^([^#]*min_gap\s*=\s*)(.*)(\s*)$/$1$min_gap$3/;
      $line =~ s/^([^#]*rank1\s*=\s*)(.*)(\s*)$/$1$rank1$3/;
      $line =~ s/^([^#]*rank2\s*=\s*)(.*)(\s*)$/$1$rank2$3/;

      print $line;
    }
  }

exit(0);

sub cleanup {
  die "Cleanup.\n";
}

sub print_help
{
  my $name = `basename $0`; chomp $name;
  print << "Help";

usage: $name [options]

  Manage compilation of SA-Hiero files and creation of ini files.
  In the default usage, the command deploys a set of files needed
  to create a system, and writes an ini for the system on stdout.

options:

  -a, --alignment <name>[=<filename>]
    Name of an alignment of a bitext (which must be specified 
    with -b unless using the -c flag).  If used with -r, the 
    alignment is removed from the deployment.  If used with -c, 
    only alignments with this name are listed.  If a filename is 
    given, then the file will be deployed using the name.

  -b, --bitext <name>[=<f file>,<e file>]
    Name of a bitext for a particular system type (which must be
    specified with -t unless using the -c flag).  If used with -r,
    the bitext is removed from the deployment.  If used with -c,
    only bitexts with the name are listed.  If a filename is given,
    then the file will be deployed using the name.

  -h, --help
    Prints this message.

  -i, --ini <filename>
    Use a specific ini file as the template for a system, rather than
    the default ini file.

  -p, --precomp-options <key1>=<value1>[,<key2>=<value2>,...,<keyN>=<valueN>]
    Set parameters of the grammar.  This must be set by $name because
    many parameters involve precomputation.  There are six keys that can
    be set: 
      max-len: maximum number of symbols (T and NT) in a grammar rule
      max-nt: maximum number of nonterminals in a grammar rule
      max-size: maximum span of a grammar rule extracted from training
      min-gap: minimum gap spanned by a nonterminal in training
      rank1: number of frequent words to precompute collocations for.
      rank2: number of super-frequent words to precompute triple 
        collocations for.
    All values must be positive integers.  If not specified, defaults are:
      max-len = 5
      max-nt = 2  (>2 not supported)
      max-size = 10
      min-gap = 2
      rank1 = 100 (>300 not recommended)
      rank2 = 10  (>10 not recommended)

  -n, --no-ini
    Do not generate an ini file on stdout.  If this option is used, then
    the requirement to specify a full system is relaxed.  Therefore, this
    option can be used when the sole objective is deployment of files.

  -o, --output-dir
    Write the compiled model to this directory.

Help
}
