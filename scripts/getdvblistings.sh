#!/bin/sh
DATADIR="`dvb --datadir`"
CONFDIR="`dvb --confdir`"

mkdir -p "$DATADIR/archives"
cd "$DATADIR"

if [ ! -f "archives/recorded-`date +%Y-%m-%d`.dat.gz" ] ; then
  cp recorded.dat "archives/recorded-`date +%Y-%m-%d`.dat" && gzip "archives/recorded-`date +%Y-%m-%d`.dat"
fi
if [ ! -f "archives/episodes-`date +%Y-%m-%d`.txt.gz" ] ; then
  cp episodes.txt "archives/episodes-`date +%Y-%m-%d`.txt" && gzip "archives/episodes-`date +%Y-%m-%d`.txt"
fi

LISTINGSFILE="archives/listings-`date +%Y-%m-%d`.xmltv"
if [ ! -f "$LISTINGSFILE.gz" ] ; then
  dvb --getxmltv "$LISTINGSFILE" --update "$LISTINGSFILE"
  gzip "$LISTINGSFILE"
fi

dvb --find-recorded-programmes-on-disk --check-recording-file --find-cards
dvb --schedule
