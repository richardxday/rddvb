#!/usr/bin/perl
use Linux::DVB::DVBT;
  
# get list of installed adapters
my @devices = Linux::DVB::DVBT->device_list();
foreach (@devices)
{
	printf "%s : adapter number: %d, frontend number: %d\n", 
		$_->{name}, $_->{adapter_num}, $_->{frontend_num};
}

# Create a dvb object using the first dvb adapter in the list
my $dvb = Linux::DVB::DVBT->new();

# Scan for channels using GB country code
$dvb->scan_from_country('GB');

# get access to tuning and channels data
my $tuning_aref = $dvb->get_tuning_info();
my $channels_aref = $dvb->get_channel_list();

# create file to write to
my $filename = '/var/dvb/channels.dat';
open(my $fh, '>', $filename) or die "Could not open file '$filename' $!";

# write channels out
foreach my $ch_href (@$channels_aref)
{
	my $chan = $ch_href->{'channel'};

	# format is [<lcn>]<name>,<freq>,<audio>,<video>
	printf $fh "[%d]%s,%d", 
		$ch_href->{'channel_num'},
		$chan,
		$tuning_aref->{'ts'}{$tuning_aref->{'pr'}{$chan}{'tsid'}}{'frequency'};
	if ($tuning_aref->{'pr'}{$chan}{'audio'} != 0) {
		printf $fh ",%d", $tuning_aref->{'pr'}{$chan}{'audio'};
	}
	if ($tuning_aref->{'pr'}{$chan}{'video'} != 0) {
		printf $fh ",%d", $tuning_aref->{'pr'}{$chan}{'video'};
	}
	printf $fh "\n";
}

close $fh;
