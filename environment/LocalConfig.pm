package LocalConfig;

use strict;
use warnings;

use base 'Exporter';
our @EXPORT = qw( qsub_args mert_memory environment_name env_default_jobs has_qsub );

use Net::Domain qw(hostname hostfqdn hostdomain domainname);

my $host = domainname;

# keys are: HOST_REGEXP, MERTMem, QSubQueue, QSubMemFlag, QSubExtraFlags
my $CCONFIG = {

  'StarCluster' => {
    'HOST_REGEXP' => qr/compute-\d+\.internal$/,
    'JobControl'  => 'qsub',
    'QSubMemFlag' => '-l mem=',
    'DefaultJobs' => 20,
  },
  'Cab' => {
    'HOST_REGEXP' => qr/cab\.ark\.cs\.cmu\.edu$|cab\.local$/,
    'JobControl' => 'qsub',
    'QSubMemFlag' => '-l mem=',
    'DefaultJobs' => 8
  },
  'LTICluster' => {
    'HOST_REGEXP' => qr/^cluster\d+\.lti\.cs\.cmu\.edu$/,
    'JobControl'  => 'qsub',
    'QSubMemFlag' => '-l h_vmem=',
    'QSubExtraFlags' => '-l walltime=0:45:00',
    'DefaultJobs' => 15,
    #'QSubQueue' => '-q long',
  },
  'UMIACS' => {
    'HOST_REGEXP' => qr/^(n|s|d).*\.umiacs\.umd\.edu$/,
    'JobControl'  => 'qsub',
    'QSubMemFlag' => '-l pmem=',
    'QSubQueue' => '-q batch',
    'QSubExtraFlags' => '-V -l walltime=144:00:00',
    'DefaultJobs' => 15,
  },
  'CLSP' => {
    'HOST_REGEXP' => qr/\.clsp\.jhu\.edu$/,
    'JobControl'  => 'qsub',
    'QSubMemFlag' => '-l mem_free=',
    'MERTMem' => '9G',
    'DefaultJobs' => 15,
  },
  'Valhalla' => {
    'HOST_REGEXP' => qr/^(thor|tyr)\.inf\.ed\.ac\.uk$/,
    'JobControl'  => 'fork',
    'DefaultJobs' => 8,
  },
  'Blacklight' => {
    'HOST_REGEXP' => qr/^(tg-login1.blacklight.psc.teragrid.org|blacklight.psc.edu|bl1.psc.teragrid.org|bl0.psc.teragrid.org)$/,
    'JobControl'  => 'fork',
    'DefaultJobs' => 32,
  },
  'Barrow/Chicago' => {
    'HOST_REGEXP' => qr/^(barrow|chicago).lti.cs.cmu.edu$/,
    'JobControl'  => 'fork',
    'DefaultJobs' => 8,
  },
  'OxfordDeathSnakes' => {
    'HOST_REGEXP' => qr/^(taipan|tiger).cs.ox.ac.uk$/,
    'JobControl'  => 'fork',
    'DefaultJobs' => 12,
  },
  'cluster.cl.uni-heidelberg.de' => {
    'HOST_REGEXP' => qr/node25/,
    'JobControl'  => 'qsub',
    'QSubMemFlag' => '-l h_vmem=',
    'DefaultJobs' => 13,
  },
  'LOCAL' => {  # LOCAL must be last in the list!!!
    'HOST_REGEXP' => qr//,
    'QSubMemFlag' => ' ',
    'JobControl'  => 'fork',
    'DefaultJobs' => 2,
  },
};

our $senvironment_name = 'LOCAL';
for my $config_key (keys %$CCONFIG) {
  my $re = $CCONFIG->{$config_key}->{'HOST_REGEXP'};
  die "Can't find HOST_REGEXP for $config_key" unless $re;
  if ($host =~ /$re/) {
    $senvironment_name = $config_key;
    last;
  }
}

our %CONFIG = %{$CCONFIG->{$senvironment_name}};
print STDERR "**Environment: $senvironment_name";
print STDERR " (has qsub)" if has_qsub();
print STDERR "\n";

sub has_qsub {
  return ($CONFIG{'JobControl'} eq 'qsub');
}

sub environment_name {
  return $senvironment_name;
}

sub env_default_jobs {
  return 1 * $CONFIG{'DefaultJobs'};
}

sub qsub_args {
  my $mem = shift @_;
  die "qsub_args requires a memory amount as a parameter, e.g. 4G" unless $mem;
  return 'qsub -V -cwd' if environment_name() eq 'StarCluster';
  my $mf = $CONFIG{'QSubMemFlag'} or die "QSubMemFlag not set for $senvironment_name";
  my $cmd = "qsub -S /bin/bash ${mf}${mem}";
  if ($CONFIG{'QSubQueue'}) { $cmd .= ' ' . $CONFIG{'QSubQueue'}; }
  if ($CONFIG{'QSubExtraFlags'}) { $cmd .= ' ' . $CONFIG{'QSubExtraFlags'}; }
  return $cmd;
}

sub mert_memory {
  return ($CONFIG{'MERTMem'} || '2G');
};

1;
