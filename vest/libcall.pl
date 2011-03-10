use IPC::Open3;
use Symbol qw(gensym);

$DUMMY_STDERR = gensym();
$DUMMY_STDIN = gensym();

# Run the command and ignore failures
sub unchecked_call {
    system("@_")
}

# Run the command and return its output, if any ignoring failures
sub unchecked_output {
    return `@_`
}

# WARNING: Do not use this for commands that will return large amounts
# of stdout or stderr -- they might block indefinitely
sub check_output {
    print STDERR "Executing and gathering output: @_\n";

    my $pid = open3($DUMMY_STDIN, \*PH, $DUMMY_STDERR, @_);
    my $proc_output = "";
    while( <PH> ) {
	$proc_output .= $_;
    }
    waitpid($pid, 0);
    # TODO: Grab signal that the process died from
    my $child_exit_status = $? >> 8;
    if($child_exit_status == 0) {
	return $proc_output;
    } else {
	print STDERR "ERROR: Execution of @_ failed.\n";
	exit(1);
    }
}

# Based on Moses' safesystem sub
sub check_call {
    print STDERR "Executing: @_\n";
    system(@_);
    my $exitcode = $? >> 8;
    if($exitcode == 0) {
	return 0;
    } elsif ($? == -1) {
	print STDERR "ERROR: Failed to execute: @_\n  $!\n";
	exit(1);

    } elsif ($? & 127) {
      printf STDERR "ERROR: Execution of: @_\n  died with signal %d, %s coredump\n",
      ($? & 127),  ($? & 128) ? 'with' : 'without';
      exit(1);

    } else {
	print STDERR "Failed with exit code: $exitcode\n" if $exitcode;
	exit($exitcode);
    }
}

sub check_bash_call {
    my @args = ( "bash", "-auxeo", "pipefail", "-c", "@_");
    check_call(@args);
}

sub check_bash_output {
    my @args = ( "bash", "-auxeo", "pipefail", "-c", "@_");
    return check_output(@args);
}

# perl module weirdness...
return 1;
