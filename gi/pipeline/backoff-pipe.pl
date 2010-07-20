#!/usr/bin/perl -w
use strict;

use Getopt::Long "GetOptions";

my @grammars;
my $OUTPUTPREFIX = './giwork/bo.hier.grammar';
my $backoff_levels = 1;
my $glue_levels = 1;
my %FREQ_HIER = ();

usage() unless &GetOptions('grmr=s@' => \ @grammars,
                           'outprefix=s' => \ $OUTPUTPREFIX,
                           'bo-lvls=i' => \ $backoff_levels,
                           'glue-lvls=i' => \ $glue_levels,
);
                           
my $OUTDIR = $OUTPUTPREFIX . '/hier';

my %grmr = ();
foreach my $grammar (@grammars) {
    $grammar =~ m/\/[^\/]*\.t(\d+)\.[^\/]*$/;
    my $grains = $1;
    $grmr{$grains} = $grammar;
}

my @index = sort keys %grmr;
$OUTDIR = $OUTDIR . join('-',@index);
my $BACKOFF_GRMR = $OUTDIR . '/backoff.gz';
my $GLUE_GRMR = $OUTDIR . '/glue.gz';
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
    safesystem("echo \"\" | gzip > $joinedgrammars");
    foreach my $i (@index) {
        my $g = $grmr{$i};
        safesystem("zcat $g | sed -r -e 's/(X[0-9]+)/\\1-$i/g' - | gzip > $g");
        safesystem("zcat $joinedgrammars $g | gzip > $joinedgrammars");
    }
}


sub extract_freqs {
    my($grmr1,$grmr2) = @_;
    print STDERR "\n!!!EXTRACTING FREQUENCIES: $grmr1->$grmr2\n";
    my $IN_COARSE = substr($grammars{$grmr1},0,index($grammars{$grmr1},".grammar/")) . "/labeled_spans.txt";
    my $IN_FINE = substr($grammars{$grmr2},0,index($grammars{$grmr2},".grammar/")) . "/labeled_spans.txt";
    my $OUT_SPANS = "$OUTDIR/labeled_spans.hier$NUM_TOPICS_COARSE-$NUM_TOPICS_FINE.txt";
    my $FREQS = "$OUTDIR/label_freq.hier$NUM_TOPICS_COARSE-$NUM_TOPICS_FINE.txt";
    my %finehier = ();
    if (-e $OUT_SPANS) {
        print STDERR "$OUT_SPANS exists, reusing...\n";
    } else {
        safesystem("paste -d ' ' $IN_COARSE $IN_FINE > $OUT_SPANS");
    }
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
    my ($grmr1, $grmr2, $freq) = @_;
    open FREQS, $freqs or die $!;
    open TMP, ">", "tmp" or die $!;
    while (<FREQS>) {
        my $coarse = m/^(\d+) \|\|\|/;
        if ($coarse == $grmr1) {
            my @finefreq = m/(\d+):(-?\d+\.?\d*)/g;
            for(my $i = 0; $i < scalar @finefreq; $i+=2) {
                my $finecat = @finefreq[$i];
                my $finefreq = @finefreq[$i+1];
                print TMP "[X$coarse-$grmr1] ||| [X$finecat-$grmr2,1]\t[1] ||| BackoffRule=$finefreq\n";
            }
        }
    }
    close TMP;
    close FREQS;
    safesystem('zcat $BACKOFF_GRMR | cat - tmp | gzip > $BACKOFF_GRMR');
}

sub add_glue_rules {
    my ($grmr) = @_;
    open TMP, ">", "tmp" or die $!;
    for my $i (0..($grmr-1)) {
        print TMP "[S] ||| [S,1] [X$i-$grmr,2] ||| [1] [2] ||| Glue=1\n";
        print TMP "[S] ||| [X$i-$grmr,1] ||| [1] ||| GlueTop=1\n";
    }
    close TMP;
    safesystem('zcat $GLUE_GRMR | cat - tmp | gzip > $GLUE_GRMR');
}

sub output_grammar_info {
    print STDOUT "GRAMMAR: \t$joinedgrammars\n";
    print STDOUT "GLUE: \t$GLUE_GRMR\n";
    print STDOUT "BACKOFF: \t$BACKOFF_GRAMMAR\n";
}
