#!/usr/bin/perl -w
#
# nt-rdf-0.1 - Convert NTriples to RDF/XML.
#
# Changelog
# ---------
# Version 0.1 - 2003.07.20:
# - First release (morten@mfd-consult.dk).
#
# Copyright (c) 2002-2003 MFD Consult, Morten Frederiksen
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#

use strict;
use Fcntl;
use POSIX qw(tmpnam);
use RDF::Redland;

my $ampify=0;
if(@ARGV && ($ARGV[0] eq '-a' || $ARGV[0] eq '--ampify')) {
	$ampify=1;
	shift;
}

# Create Redland objects...
my $storage=new RDF::Redland::Storage('hashes',
		'dummy',"new='yes',hash-type='memory'")
		or die ($0.': unable to create RDF::Redland::Storage');
my $model=new RDF::Redland::Model($storage,'')
		or die ($0.': unable to create RDF::Redland::Model');
my $parser=new RDF::Redland::Parser('ntriples')
		or die ($0.': unable to create RDF::Redland::Parser');
my $serializer=new RDF::Redland::Serializer('rdfxml')
		or die ($0.': unable to create RDF::Redland::Serializer');

# Create temporary file.
my $tempFile='';
do { $tempFile=tmpnam() }
        until sysopen(TEMP,$tempFile,O_RDWR|O_CREAT|O_EXCL);
while (<>)
{
	$_=~s/&/&amp;/g if($ampify);
	$_=~s/([\x00-\x08\x0B-\x0C\x0E-\x1F\x7F-\xFF])/"\\u".sprintf("00%2x",ord($1))/eg;
	print TEMP $_;
};
close(TEMP);

# Parse...
my $uri=new RDF::Redland::URI('file:'.$tempFile);
!$parser->parse_into_model($uri,new RDF::Redland::URI('./'),$model)
		or die ($0.': unable to parse input ntriples');

# Serialize
!$serializer->serialize_model_to_file($tempFile,undef,$model)
		or die ($0.': unable to serialize model');
open (TEMP,'<'.$tempFile)
		or die ($0.': unable to open serialized model');
my %bnodes=();
while (<TEMP>)
{
	$_=~s/:mmFilm(.+?exif\/ns#)focalLengthIn35(".+?):mmFilm/:focalLengthIn35mmFilm$1$2:focalLengthIn35mmFilm/g;
	$_=~s/(\s+rdf:about=").\/(">)/$1$2/g;
	if ($_=~/^(\s*<.+rdf:XXXnodeID=")([^"]+)(".*>\s*)$/)
	{
		if (!defined($bnodes{$2}))
		{
			my $b=new RDF::Redland::Node();
			$bnodes{$2}=$b->blank_identifier;
			$b=undef;
		};
		$_=$1.$bnodes{$2}.$3;
	};
	print $_;
};
close(TEMP);
unlink($tempFile);

exit 0;

$serializer=undef;
$uri=undef;
$parser=undef;
$model=undef;
$storage=undef;
