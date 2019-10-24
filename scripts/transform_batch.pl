#!/usr/bin/env perl
# Invoke vtxremap on all graphs in the given directory

use strict;
use warnings;
use File::Basename;

# Usage
if(scalar(@ARGV) != 2 || grep($_ =~ /^(-h|--help|-?)$/, @ARGV)) {
	print("Transform all vertices in the given directory into a dense domain\n");
	print("Usage: $0 <inputdir> <outputdir>\n");
	exit(0);
}

my $inputdir = $ARGV[0];
my $outputdir = $ARGV[1];
my $vtxremap = dirname(__FILE__) . "/../build/vtxremap"; # path to the executable vtxremap

die("The executable is not present in $vtxremap") if( ! -e $vtxremap );

system("mkdir -p $outputdir"); # just in case it doesn't already exist

foreach my $input (glob("$inputdir/*.properties")) {
	my $output = $outputdir . "/" . ( basename($input) =~ s/\.properties/-dense.properties/r );
	if(! -e $output){
		my $cmd = "$vtxremap -c $input $output";
		print("$cmd\n");
		system("$cmd");
	} else {
		print("The file $output already exists, skipped\n");
	}
	
}

print("Done\n");