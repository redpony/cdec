#!/usr/bin/perl -w
#hooks up two processes, 2nd of which has one line of output per line of input, expected by the first, which starts off the communication

# if you don't know how to fork/exec in a C program, this could be helpful under limited cirmustances (would be ok to liaise with sentserver)

#WARNING: because it waits for the result from command 2 after sending every line, and especially if command 1 does the same, using sentserver as command 2 won't actually buy you any real parallelism.

use strict;
use IPC::Open2;
use POSIX qw(pipe dup2 STDIN_FILENO STDOUT_FILENO);

my $quiet=!$ENV{DEBUG};
$quiet=1 if $ENV{QUIET};
sub info {
    local $,=' ';
    print STDERR @_ unless $quiet;
}

my $mode='CROSS';
my $ser='DIRECT';
$mode='PIPE' if $ENV{PIPE};
$mode='SNAKE' if $ENV{SNAKE};
$mode='CROSS' if $ENV{CROSS};
$ser='SERIAL' if $ENV{SERIAL};
$ser='DIRECT' if $ENV{DIRECT};
$ser='SERIAL' if $mode eq 'SNAKE';
info("mode: $mode\n");
info("connection: $ser\n");


my @c1;
if (scalar @ARGV) {
    do {
        push @c1,shift
    } while scalar @ARGV && $c1[$#c1] ne '--';
}
pop @c1;
my @c2=@ARGV;
@ARGV=();
(scalar @c1 && scalar @c2) || die qq{
usage: $0 cmd1 args -- cmd2 args
all options are environment variables.
DEBUG=1 env var enables debugging output.
CROSS=1 hooks up two processes, 2nd of which has one line of output per line of input, expected by the first, which starts off the communication.  crosses stdin/stderr of cmd1 and cmd2 line by line (both must flush on newline and output.  cmd1 initiates the conversation (sends the first line).    default: attempts to cross stdin/stdout of c1 and c2 directly (via two unidirectional posix pipes created before fork).
SERIAL=1: (no parallelism possible) but lines exchanged are logged if DEBUG.
if SNAKE then stdin -> c1 -> c2 -> c1 -> stdout.
if PIPE then stdin -> c1 -> c2 -> stdout (same as shell c1|c2, but with SERIAL you can see the intermediate in real time; you could do similar with c1 | tee /dev/fd/2 |c2.
DIRECT=1 (default) will override SERIAL=1.
CROSS=1 (default) will override SNAKE or PIPE.
};

info("1 cmd:",@c1,"\n");
info("2 cmd:",@c2,"\n");

sub lineto {
    select $_[0];
    $|=1;
    shift;
    print @_;
}

if ($ser eq 'SERIAL') {
    my ($R1,$W1,$R2,$W2);
    my $c1p=open2($R1,$W1,@c1); # Open2 R W backward from Open3.
    my $c2p=open2($R2,$W2,@c2);
    if ($mode eq 'CROSS') {
        while(<$R1>) {
            info("1:",$_);
            lineto($W2,$_);
            last unless defined ($_=<$R2>);
            info("1|2:",$_);
            lineto($W1,$_);
        }
    } else {
        my $snake=$mode eq 'SNAKE';
        while(<STDIN>) {
            info("IN:",$_);
            lineto($W1,$_);
            last unless defined ($_=<$R1>);
            info("IN|1:",$_);
            lineto($W2,$_);
            last unless defined ($_=<$R2>);
            info("IN|1|2:",$_);
            if ($snake) {
                lineto($W1,$_);
                last unless defined ($_=<$R1>);
                info("IN|1|2|1:",$_);
            }
            lineto(*STDOUT,$_);
        }
    }
} else {
    info("DIRECT mode\n");
    my @rw1=POSIX::pipe();
    my @rw2=POSIX::pipe();
    my $pid=undef;
    $SIG{CHLD} = sub { wait };
    while (not defined ($pid=fork())) {
        sleep 1;
    }
    my $pipe = $mode eq 'PIPE';
    unless ($pipe) {
        POSIX::close(STDOUT_FILENO);
        POSIX::close(STDIN_FILENO);
    }
    if ($pid) {
        POSIX::dup2($rw1[1],STDOUT_FILENO);
        POSIX::dup2($rw2[0],STDIN_FILENO) unless $pipe;
        exec @c1;
    } else {
        POSIX::dup2($rw2[1],STDOUT_FILENO) unless $pipe;
        POSIX::dup2($rw1[0],STDIN_FILENO);
        exec @c2;
    }
    while (wait()!=-1) {}
}
