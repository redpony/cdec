#!/usr/bin/env perl

# Author: Adam Lopez
# 
# This script takes a command that processes input
# from stdin one-line-at-time, and parallelizes it
# on the cluster using David Chiang's sentserver/
# sentclient architecture.  
# 
# Prerequisites: the command *must* read each line
# without waiting for subsequent lines of input
# (for instance, a command which must read all lines
# of input before processing will not work) and 
# return it to the output *without* buffering 
# multiple lines.

use Getopt::Long;
use IPC::Open2;
use strict;
use POSIX ":sys_wait_h";

my $recycle_clients;		# spawn new clients when previous ones terminate
my $stay_alive;			# dont let server die when having zero clients
my $joblist = "";
my $errordir;
my $multiline;
my @files_to_stage;
my $verbose = 1;
my $nodelist;
my $user = $ENV{"USER"};
my $pmem = "2g";

sub print_help;

# Process command-line options
if (GetOptions(
			"stay-alive" => \$stay_alive,
			"recycle-clients" => \$recycle_clients,
			"error-dir=s" => \$errordir,
			"multi-line" => \$multiline,
			"file=s" => \@files_to_stage,
			"verbose" => \$verbose,
			"nodelist=s" => \$nodelist,
			"pmem=s" => \$pmem
) == 0 || @ARGV == 0){
	print_help();
}

my @nodes = grep(/^[cd]\d\d$/, split(/\n/, `pbsnodes -a`));
if ($nodelist){
	@nodes = split(/ /, $nodelist);
} 

if ($verbose){
	print STDERR "Compute nodes: @nodes\n";
}

my $cmd = "";
for my $arg (@ARGV){
	if ($arg=~ /\s/){
		$cmd .= "\"$arg\" ";
	} else {
		$cmd .= "$arg "
	}
}

if ($errordir){
	`mkdir -p $errordir`;
}

if ($verbose){ print STDERR "Parallelizing: $cmd\n\n"; }

# set cleanup handler 
my @cleanup_cmds;
sub cleanup;
sub cleanup_and_die;
$SIG{INT} = "cleanup_and_die";
$SIG{TERM} = "cleanup_and_die"; 
$SIG{HUP} = "cleanup_and_die";

# other subs:
sub numof_live_jobs;
sub launch_job_on_node;


# vars
my $mydir = `dirname $0`; chomp $mydir;
my $sentserver = "$mydir/sentserver";
my $sentclient = "$mydir/sentclient";
my $host = `hostname`;
chomp $host;

my $executable = $cmd;  
$executable =~ s/^\s*(\S+)($|\s.*)/$1/;
$executable=`basename $executable`;
chomp $executable;

# find open port
my $port = 50300;
if ($verbose){ print STDERR "Testing port $port...";}
while (`netstat -l | grep $port`){
	if ($verbose){ print STDERR "port is busy\n";}
	$port++;
	if ($port > 50400){
		die "Unable to find open port\n";
	}
	if ($verbose){ print STDERR "Testing port $port... "; }
}
if ($verbose){
	print STDERR "port is available\n";
}

srand;
my $key = int(rand()*1000000);

my $multiflag = "";
if ($multiline){ $multiflag = "-m"; print STDERR "expecting multiline output.\n"; }
my $stay_alive_flag = "";
if ($stay_alive){ $stay_alive_flag = "--stay-alive"; print STDERR "staying alive while no clients are connected.\n"; }

my %node_count;
my $script = "";

# fork == one thread runs the sentserver, while the
# other spawns the sentclient commands.
if (my $pid = fork){
	  sleep 5; # give other thread time to start sentserver
    $script .= "wait\n";
	$script .= "$sentclient $host:$port:$key $cmd\n";
	if ($verbose){
		print STDERR "Client script:\n====\n";
		print STDERR $script;
		print STDERR "====\n";
	}
	for my $node (@nodes){
		launch_job_on_node($node);
	}
	if ($recycle_clients) {
		my $ret;
		my $livejobs;
		while (1) {
			$ret = waitpid($pid, WNOHANG);
			#print STDERR "waitpid $pid ret = $ret \n";
			if ($ret != 0) {last; } # break
			$livejobs = numof_live_jobs();
			if ( $#nodes >= $livejobs ) {	# a client terminated
				my $numof_nodes = scalar @nodes;
				print STDERR "num of requested nodes = $numof_nodes; num of currently live jobs = $livejobs; Client terminated - launching another.\n";
				launch_job_on_node(1);	# TODO: support named nodes
			} else {
				sleep (60);
			}
		}
	}
	waitpid($pid, 0);
	cleanup();
} else {
#	my $todo = "$sentserver -k $key $multiflag $port ";
	my $todo = "$sentserver -k $key $multiflag $port $stay_alive_flag ";
	if ($verbose){ print STDERR "Running: $todo\n"; }
	my $rc = system($todo);
	if ($rc){
		die "Error: sentserver returned code $rc\n";
	}
}

sub numof_live_jobs {
	my @livejobs = grep(/$joblist/, split(/\n/, `qstat`));
	return ($#livejobs + 1);
}

sub launch_job_on_node {
		my $node = $_[0];

		my $errorfile = "/dev/null";
		my $outfile = "/dev/null";
		unless (exists($node_count{$node})){
			$node_count{$node} = 0;
		}
		my $node_count = $node_count{$node};
		$node_count{$node}++;
		my $clientname = $executable;
		$clientname =~ s/^(.{4}).*$/$1/;
		$clientname = "$clientname.$node.$node_count";
		if ($errordir){
			$errorfile = "$errordir/$clientname.ER"; 
			$outfile = "$errordir/$clientname.OU";
		}
		my $todo = "qsub -l mem_free=9G -N $clientname -o $outfile -e $errorfile";
		if ($verbose){ print STDERR "Running: $todo\n"; }
		local(*QOUT, *QIN);
		open2(\*QOUT, \*QIN, $todo);
		print QIN $script;
		close QIN;
		while (my $jobid=<QOUT>){
			chomp $jobid;
			if ($verbose){ print STDERR "Launched client job: $jobid"; }
			push(@cleanup_cmds, "`qdel $jobid 2> /dev/null`");
			$jobid =~ s/^(\d+)(.*?)$/\1/g;
			print STDERR "short job id $jobid\n";
			if ($joblist == "") { $joblist = $jobid; }
			else {$joblist = $joblist . "\|" . $jobid; }
		}
		close QOUT;
}


sub cleanup_and_die {
	cleanup();
	die "\n";
}

sub cleanup {
	if ($verbose){ print STDERR "Cleaning up...\n"; }
	for $cmd (@cleanup_cmds){
		if ($verbose){ print STDERR "  $cmd\n"; }
		eval $cmd;
	}
	if ($verbose){ print STDERR "Cleanup finished.\n"; }
}

sub print_help
{
	my $name = `basename $0`; chomp $name;
	print << "Help";

usage: $name [options]

	Automatic black-box parallelization of commands.

options:

	-e, --error-dir <dir>
		Retain output files from jobs in <dir>, rather
		than silently deleting them.
	
	-m, --multi-line
		Expect that command may produce multiple output
		lines for a single input line.  $name makes a
		reasonable attempt to obtain all output before
		processing additional inputs.  However, use of this
		option is inherently unsafe.

	-v, --verbose
		Print diagnostic informatoin on stderr.

	-n, --nodelist
		Space-delimited list of nodes to request.  There is
		one qsub command per node.

	-p, --pmem
		pmem setting for each job.

Help
}
