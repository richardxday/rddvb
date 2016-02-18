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

LISTINGSFILE="archives/listings-`date +%Y-%m-%d`.xmltv"
if [ ! -f "$LISTINGSFILE.gz" ] ; then
  tv_grab_uk_atlas --days 14 >"$LISTINGSFILE"
  dvb --update "$LISTINGSFILE"
  gzip "$LISTINGSFILE"
fi

dvb --find-recorded-programmes-on-disk --check-recording-file --cards
dvb --schedule

LISTINGSFILE="archives/listings-sky-`date +%Y-%m-%d`.xmltv"
if [ ! -f "$LISTINGSFILE.gz" ] ; then
  tv_grab_uk_rt --config-file ~/.xmltv/tv_grab_uk_rt-sky.conf >"$LISTINGSFILE"
  dvb --read "$LISTINGSFILE" --update-dvb-channels --write "skylistings.dat"
  checksky.sh
  gzip "$LISTINGSFILE"
fi
