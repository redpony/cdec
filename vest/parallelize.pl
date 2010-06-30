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
use Cwd qw(getcwd);

my $recycle_clients;		# spawn new clients when previous ones terminate
my $stay_alive;			# dont let server die when having zero clients
my $joblist = "";
my $errordir="";
my $multiline;
my @files_to_stage;
my $verbose = 1;
my $numnodes;
my $user = $ENV{"USER"};
my $pmem = "9g";
my $basep=50300;
my $randp=300;
my $tryp=50;
my $no_which;
my $no_cd;

my $DEBUG=$ENV{DEBUG};
sub debug {
    if ($DEBUG) {
        my ($package, $filename, $line) = caller;
        print STDERR "$filename($line): ",join(' ',@_);
    }
}
sub abspath($) {
    my $p=shift;
    my $a=`readlink -f $p`;
    chomp $a;
    $a
}
my $is_shell_special=qr.[ \t\n\\><|&;"'`~*?{}$!()].;
my $shell_escape_in_quote=qr.[\\"\$`!].;
sub escape_shell {
    my ($arg)=@_;
    return undef unless defined $arg;
    return '""' unless $arg;
    if ($arg =~ /$is_shell_special/) {
        $arg =~ s/($shell_escape_in_quote)/\\$1/g;
        return "\"$arg\"";
    }
    return $arg;
}
my $tailn=5;
sub preview_files {
    my $n=shift;
    $n=$tailn unless defined $n;
    my $fn=join(' ',map {escape_shell($_)} @_);
    my $cmd="tail -n $n $fn";
    my $text=`$cmd`;
    debug($cmd,$text);
    $text
}
sub prefix_dirname($) {
    #like `dirname but if ends in / then return the whole thing
    local ($_)=@_;
    if (/\/$/) {
        $_;
    } else {
        s#/[^/]$##;
        $_ ? $_ : '';
    }
}
sub extend_path($$;$$) {
    my ($base,$ext,$mkdir,$baseisdir)=@_;
    if (-d $base) {
        $base.="/";
    } else {
        my $dir;
        if ($baseisdir) {
            $dir=$base;
            $base.='/' unless $base =~ /\/$/;
        } else {
            $dir=prefix_dirname($base);
        }
        system("mkdir -p '$dir'") if $mkdir;
    }
    return $base.$ext;
}

my $abscwd=abspath(&getcwd);
sub print_help;

# Process command-line options
unless (GetOptions(
			"stay-alive" => \$stay_alive,
			"recycle-clients" => \$recycle_clients,
			"error-dir=s" => \$errordir,
			"multi-line" => \$multiline,
			"file=s" => \@files_to_stage,
			"verbose" => \$verbose,
			"jobs=i" => \$numnodes,
			"pmem=s" => \$pmem,
        "baseport=i" => \$basep,
        "iport=i" => \$randp, #ugly option name so first letter doesn't conflict
        "no-which!" => \$no_which,
            "no-cd!" => \$no_cd,
) && scalar @ARGV){
	print_help();
    die "bad options.";
}

my $cmd = "";
my $prog=shift;
if ($no_which) {
    $cmd=$prog;
} else {
    $cmd=`which $prog`;
    chomp $cmd;
    die "$prog not found - $cmd" unless $cmd;
}
#$cmd=abspath($cmd);
for my $arg (@ARGV) {
    $cmd .= " ".escape_shell($arg);
}

die "Please specify a command to parallelize\n" if $cmd eq '';

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
srand;
my $port = 50300+int(rand($randp));
my $endp=$port+$tryp;
sub listening_port_lines {
    my $quiet=$verbose?'':'2>/dev/null';
    `netstat -a -n $quiet | grep LISTENING | grep -i tcp`
}
my $netstat=&listening_port_lines;

if ($verbose){ print STDERR "Testing port $port...";}

while ($netstat=~/$port/ || &listening_port_lines=~/$port/){
	if ($verbose){ print STDERR "port is busy\n";}
	$port++;
	if ($port > $endp){
		die "Unable to find open port\n";
	}
	if ($verbose){ print STDERR "Testing port $port... "; }
}
if ($verbose){
	print STDERR "port $port is available\n";
}

my $key = int(rand()*1000000);

my $multiflag = "";
if ($multiline){ $multiflag = "-m"; print STDERR "expecting multiline output.\n"; }
my $stay_alive_flag = "";
if ($stay_alive){ $stay_alive_flag = "--stay-alive"; print STDERR "staying alive while no clients are connected.\n"; }

my %node_count;
my $script = "";
my $cdcmd=$no_cd ? '' : "cd '$abscwd'\n";
# fork == one thread runs the sentserver, while the
# other spawns the sentclient commands.
if (my $pid = fork){
	  sleep 2; # give other thread time to start sentserver
    $script =
        qq{wait
$cdcmd$sentclient $host:$port:$key $cmd
};
	if ($verbose){
		print STDERR "Client script:\n====\n";
		print STDERR $script;
		print STDERR "====\n";
	}
	for (my $jobn=0; $jobn<$numnodes; $jobn++){
		launch_job_on_node(1);
	}
	if ($recycle_clients) {
		my $ret;
		my $livejobs;
		while (1) {
			$ret = waitpid($pid, WNOHANG);
			#print STDERR "waitpid $pid ret = $ret \n";
			if ($ret != 0) {last; } # break
			$livejobs = numof_live_jobs();
			if ($numnodes >= $livejobs ) {	# a client terminated
				print STDERR "num of requested nodes = $numnodes; num of currently live jobs = $livejobs; Client terminated - launching another.\n";
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
my (@errors,@outs,@cmds);
my $scriptfile=extend_path("$errordir","$executable.sh",1,1);
if ($errordir) {
    open SF,">",$scriptfile || die;
    print SF "$cdcmd$cmd\n";
    close SF;
    chmod 0755,$scriptfile;
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
            push @errors,$errorfile;
            push @outs,$outfile;
		}
		my $todo = "qsub -l mem_free=$pmem -N $clientname -o $outfile -e $errorfile";
        push @cmds,$todo;

		if ($verbose){ print STDERR "Running: $todo\n"; }
		local(*QOUT, *QIN);
		open2(\*QOUT, \*QIN, $todo);
		print QIN $script;
		close QIN;
		while (my $jobid=<QOUT>){
			chomp $jobid;
			if ($verbose){ print STDERR "Launched client job: $jobid"; }
			$jobid =~ s/^(\d+)(.*?)$/\1/g;
            $jobid =~ s/^Your job (\d+) .*$/\1/;
			print STDERR " short job id $jobid\n";
            if ($verbose){
                print STDERR "-e dir: $errordir\n" if $errordir;
                print STDERR "cd: $abscwd\n";
                print STDERR "cmd: $cmd\n";
            }
			if ($joblist == "") { $joblist = $jobid; }
			else {$joblist = $joblist . "\|" . $jobid; }
            my $cleanfn="`qdel $jobid 2> /dev/null`";
			push(@cleanup_cmds, $cleanfn);
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
    print STDERR "outputs:\n",preview_files(undef,@outs),"\n";
    print STDERR "errors:\n",preview_files(undef,@errors),"\n";
    print STDERR "cmd:\n",$cmd,"\n";
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

	-j, --jobs
    Number of jobs to use.

	-p, --pmem
		pmem setting for each job.

Help
}
