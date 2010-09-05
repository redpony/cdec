#!/usr/bin/perl -w
#parallelize mbr computation from k-best list
#Example usage : ./mbr.pl <kbest input file> <mbr output directory>
use strict;
use File::Temp;
use FileHandle;
use Cwd qw(getcwd);

sub create_qsub;
my $SCRIPT_DIR; BEGIN { use Cwd qw/ abs_path /; use File::Basename; $SCRIPT_DIR = dirname(abs_path($0)); push @INC, $SCRIPT_DIR, "$SCRIPT_DIR/../environment"; }
my $MBR = "$SCRIPT_DIR/mbr_kbest";

die unless -x $MBR;
my $kbest = shift @ARGV;
die "First parameter must be a kbest file!" unless -r $kbest;

#create directory to store seperate kbest files and mbr output
my $dir = shift @ARGV;
if (-e $dir)
{ die "$dir exists\n";
}{ mkdir $dir or die;}
die "Second parameter must be a directory!" unless -d $dir;

#my @a = grep { /^--weights$/ } @ARGV;
#die "Please specify a weights file with --weights" unless scalar @a;

open SCORE, "< $kbest" or next;
my @lines = <SCORE>;
my $num = @lines;
my %sent_id=();
my $cdir = `pwd`;

my %ids=();

my $fh;
my $file_count=0;

#temp file to store qsub commands
my $fn = File::Temp::tempnam("/tmp", "mbr-");
mkdir $fn or die "Couldn't create $fn: $!";

#split the kbest list into per sentence files
foreach my $line (@lines)
{
    my @parts = split(/ /, $line);
    my $sentid = $parts[0];
    if ($sent_id{$sentid})
    {
	if(defined $fh){ print $fh $line;}
    }
    else
    {
	if (defined $fh){ 

	    $fh->close;
	    #create qsub entry
	    create_qsub("$file_count");
	    $file_count++;
	}
	
	$fh=FileHandle->new(">$dir/kbest.mbr.$file_count");
	print $fh ($line);
	$sent_id{$sentid}++;

    }
}

$fh->close;
#create last qsub entry
create_qsub("$file_count");
$file_count++;


sleep 1;

print STDERR "Waiting...\n";
my $flag;
do {
  sleep 5;
  $flag = undef;
  my $stat = `qstat`;
  my @lines = split /\n/, $stat;
  for my $ln (@lines) {
    my ($x, @rest) = split /\./, $ln;
    if ($ids{$x}) { $flag = 1; }
  }
} while ($flag);

#consolidate mbr output into one kbest file
for (my $i = 0; $i < $file_count; $i++) {
  open F, "<$dir/$i.mbr-txt" or die "Couldn't read $dir/$i.mbr-txt: $!";
  while(<F>) {
    print;
  }
  close F;
}

#`rm -rf $fn`;


sub create_qsub {

    my $file_n = shift;
    my $sfn = "$fn/$file_n.mbr";
 
  open F, ">$sfn" or die "Couldn't create $sfn: $!";
  print F "cd $cdir";
  print F "$MBR < $dir/kbest.mbr.$file_n @ARGV -L > $dir/$file_n.mbr-txt\n";
  close F;
 # `sleep 10`;
  my $o = `qsub -q batch -l pmem=1000mb,walltime=06:00:00 $sfn -k n -e /dev/null`;
  #my $o = `qsub -q batch -l pmem=1000mb,walltime=06:00:00 $sfn -k n`;
  my ($x, @rest) = split /\./, $o;
  $ids{$x}=1;

}
