#!/usr/bin/perl -w
use strict;

use Getopt::Long "GetOptions";

my @grammars;
my $OUTPUTPREFIX = './giwork/bo.hier.grammar';
safemkdir($OUTPUTPREFIX);
my $backoff_levels = 1;
my $glue_levels = 1;

usage() unless &GetOptions('grmr=s@' => \ @grammars,
                           'outprefix=s' => \ $OUTPUTPREFIX,
                           'bo-lvls=i' => \ $backoff_levels,
                           'glue-lvls=i' => \ $glue_levels,
);
                           
my $OUTDIR = $OUTPUTPREFIX . '/hier';
print STDERR "@grammars\n";


my %grmr = ();
foreach my $grammar (@grammars) {
    $grammar =~ m/\/[^\/]*\.t(\d+)\.[^\/]*/;
    $grmr{$1} = $grammar;
}

my @index = sort keys %grmr;
$OUTDIR = $OUTDIR . join('-',@index);
safemkdir($OUTDIR);
my $BACKOFF_GRMR = $OUTDIR . '/backoff.hier.gz';
safesystem("echo \"\" | gzip > $BACKOFF_GRMR");
my $GLUE_GRMR = $OUTDIR . '/glue.hier.gz';
safesystem("echo \"\" | gzip > $GLUE_GRMR");
my $joinedgrammars = $OUTDIR . '/grammar.hier.gz';

join_grammars();

for my $i (0..(scalar @index)-2) {
    my $freqs = extract_freqs($index[$i], $index[$i+1]);
    if ($i < $backoff_levels) {
        create_backoff_rules($index[$i],$index[$i+1],$freqs);
    }
    if ($i < $glue_levels) {
        add_glue_rules($index[$i]);
    }
}

output_grammar_info();


sub usage {
  print <<EOT;

Usage: $0 [OPTIONS] corpus.fr-en-al

Induces a grammar using Pitman-Yor topic modeling or Posterior Regularisation.

EOT
  exit 1;
};

sub safemkdir {
  my $dir = shift;
  if (-d $dir) { return 1; }
  return mkdir($dir);
}


sub safesystem {
  print STDERR "Executing: @_\n";
  system(@_);
  if ($? == -1) {
      print STDERR "ERROR: Failed to execute: @_\n  $!\n";
      exit(1);
  }
  elsif ($? & 127) {
      printf STDERR "ERROR: Execution of: @_\n  died with signal %d, %s coredump\n",
          ($? & 127),  ($? & 128) ? 'with' : 'without';
      exit(1);
  }
  else {
    my $exitcode = $? >> 8;
    print STDERR "Exit code: $exitcode\n" if $exitcode;
    return ! $exitcode;
  }
}


sub join_grammars {
    print STDERR "\n!!! JOINING GRAMMARS\n";
    if(-e $joinedgrammars) {
        print STDERR "$joinedgrammars exists, reusing...\n";
        return;
    }
    safesystem("echo \"\" | gzip > $joinedgrammars");
    foreach my $i (@index) {
        my $g = $grmr{$i};
        safesystem("zcat $g | sed -r -e 's/X([0-9]+)/X$i\\1/g' - | gzip > $g.2.gz");
        safesystem("zcat $joinedgrammars $g.2.gz | gzip > $joinedgrammars.2.gz");
        safesystem("mv $joinedgrammars.2.gz $joinedgrammars");
    }
}


sub extract_freqs {
    my($grmr1,$grmr2) = @_;
    print STDERR "\n!!!EXTRACTING FREQUENCIES: $grmr1->$grmr2\n";
    my $IN_COARSE = substr($grmr{$grmr1},0,index($grmr{$grmr1},".grammar/")) . "/labeled_spans.txt";
    my $IN_FINE = substr($grmr{$grmr2},0,index($grmr{$grmr2},".grammar/")) . "/labeled_spans.txt";
    my $OUT_SPANS = "$OUTDIR/labeled_spans.hier$grmr1-$grmr2.txt";
    my $FREQS = "$OUTDIR/label_freq.hier$grmr1-$grmr2.txt";
    if(-e $OUT_SPANS && -e $FREQS) {
        print STDERR "$OUT_SPANS exists, reusing...\n";
        print STDERR "$FREQS exists, reusing...\n";
        return $FREQS;
    }
    
    safesystem("paste -d ' ' $IN_COARSE $IN_FINE > $OUT_SPANS");
    
    my %FREQ_HIER = ();
    my %finehier = ();
    
    open SPANS, $OUT_SPANS or die $!;
    while (<SPANS>) {
        my ($tmp, $coarse, $fine) = split /\|\|\|/;
        my @coarse_spans = $coarse =~ /\d+-\d+:X(\d+)/g;
        my @fine_spans = $fine =~ /\d+-\d+:X(\d+)/g;
        
        foreach my $i (0..(scalar @coarse_spans)-1) {
            my $coarse_cat = $coarse_spans[$i];
            my $fine_cat = $fine_spans[$i];
            
            $FREQ_HIER{$coarse_cat}{$fine_cat}++;
        }
    }
    close SPANS;
    foreach (values %FREQ_HIER) {
        my $coarse_freq = $_;
        my $total = 0;
        $total+=$_ for (values %{ $coarse_freq });
        $coarse_freq->{$_}=log($coarse_freq->{$_}/$total) for (keys %{ $coarse_freq });
    }
    open FREQS, ">", $FREQS or die $!;
    foreach my $coarse_cat (keys %FREQ_HIER) {
        print FREQS "$coarse_cat |||";
        foreach my $fine_cat (keys %{$FREQ_HIER{$coarse_cat}}) {
            my $freq = $FREQ_HIER{$coarse_cat}{$fine_cat};
            print FREQS " $fine_cat:$freq";
            if(! exists $finehier{$fine_cat} || $finehier{$fine_cat} < $freq) {
               $finehier{$fine_cat} = $coarse_cat;
            }  
        }
        print FREQS "\n";
    }
#    foreach my $fine_cat (keys %finehier) {
#        print FREQS "$fine_cat -> $finehier{$fine_cat}\n";
#    }
    close FREQS;
    return $FREQS;
}


sub create_backoff_rules {
    print STDERR "\n!!! CREATING BACKOFF RULES\n";
    my ($grmr1, $grmr2, $freq) = @_;
    my $OUTFILE = "$OUTDIR/backoff.hier$grmr1-$grmr2.txt";
    if(-e $OUTFILE) {
        print STDERR "$OUTFILE exists, reusing...\n";
        return;
    }
    open FREQS, $freq or die $!;
    open TMP, ">", $OUTFILE or die $!;
    while (<FREQS>) {
        my $line = $_;
        $line = m/^(\d+) \|\|\| (.+)$/;
        my $coarse = $1;
        $line = $2;
        my @finefreq = $line =~ m/(\d+):(\S+)/g;
        for(my $i = 0; $i < scalar @finefreq; $i+=2) {
            my $finecat = $finefreq[$i];
            my $finefreq = $finefreq[$i+1];
            print TMP "[X$grmr1$coarse] ||| [X$grmr2$finecat,1]\t[1] ||| BackoffRule=$finefreq\n";
        }
    }
    close TMP;
    close FREQS;
    safesystem("zcat $BACKOFF_GRMR | cat - $OUTFILE | gzip > $BACKOFF_GRMR.2.gz");
    safesystem("mv $BACKOFF_GRMR.2.gz $BACKOFF_GRMR");
}

sub add_glue_rules {
    print STDERR "\n!!! CREATING GLUE RULES\n";
    my ($grmr) = @_;
    my $OUTFILE = "$OUTDIR/glue.$grmr.gz";
    if (-e $OUTFILE) {
        print STDERR "$OUTFILE exists, reusing...\n";
        return;
    }
    open TMP, ">", $OUTFILE or die $!;
    for my $i (0..($grmr-1)) {
        print TMP "[S] ||| [S,1] [X$grmr$i,2] ||| [1] [2] ||| Glue=1\n";
        print TMP "[S] ||| [X$grmr$i,1] ||| [1] ||| GlueTop=1\n";
    }
    close TMP;
    safesystem("zcat $GLUE_GRMR | cat - $OUTFILE | gzip > $GLUE_GRMR.2.gz");
    safesystem("mv $GLUE_GRMR.2.gz $GLUE_GRMR");
}

sub output_grammar_info {
    print STDERR "\n!!! GRAMMAR INFORMATION\n";
    print STDOUT "GRAMMAR: \t$joinedgrammars\n";
    print STDOUT "GLUE: \t$GLUE_GRMR\n";
    print STDOUT "BACKOFF: \t$BACKOFF_GRMR\n";
}
