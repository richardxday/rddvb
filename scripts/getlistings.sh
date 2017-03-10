#!/bin/sh
DATADIR="`dvb --datadir`"
CONFDIR="`dvb --confdir`"

cd "$DATADIR"

if [ ! -f "archives/recorded-`date +%Y-%m-%d`.dat.gz" ] ; then
  cp recorded.dat "archives/recorded-`date +%Y-%m-%d`.dat" && gzip "archives/recorded-`date +%Y-%m-%d`.dat"
fi
if [ ! -f "archives/episodes-`date +%Y-%m-%d`.txt.gz" ] ; then
  cp episodes.txt "archives/episodes-`date +%Y-%m-%d`.txt" && gzip "archives/episodes-`date +%Y-%m-%d`.txt"
fi

gendvbgraph.sh

LISTINGSFILE="archives/listings-`date +%Y-%m-%d`.xmltv"
if [ ! -f "$LISTINGSFILE.gz" ] ; then
  tv_grab_sd_json --days 14 >"$LISTINGSFILE"
  dvb --update "$LISTINGSFILE"
  gzip "$LISTINGSFILE"
fi

dvb --find-recorded-programmes-on-disk --check-recording-file --find-cards
dvb --schedule
