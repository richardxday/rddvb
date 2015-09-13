#!/bin/sh
DATADIR="`dvb --datadir`"
CONFDIR="`dvb --confdir`"

cd "$DATADIR"

cp recorded.dat "archives/recorded-`date +%Y-%m-%d`.dat" && gzip "archives/recorded-`date +%Y-%m-%d`.dat"
cp episodes.txt "archives/episodes-`date +%Y-%m-%d`.txt" && gzip "archives/episodes-`date +%Y-%m-%d`.txt"

LISTINGSFILE="archives/listings-`date +%Y-%m-%d`.xmltv"
if [ ! -f "$LISTINGSFILE.gz" ] ; then
  tv_grab_uk_rt >"$LISTINGSFILE"
  dvb --update "$LISTINGSFILE"
  gzip "$LISTINGSFILE"
fi
dvb --find-recorded-programmes-on-disk --check-recording-file
dvb --schedule

LISTINGSFILE="archives/listings-sky-`date +%Y-%m-%d`.xmltv"
if [ ! -f "$LISTINGSFILE.gz" ] ; then
  tv_grab_uk_rt --config-file ~/.xmltv/tv_grab_uk_rt-sky.conf >"$LISTINGSFILE"
  dvb --read "$LISTINGSFILE" --update-dvb-channels --write "skylistings.dat"
  checksky.sh
  gzip "$LISTINGSFILE"
fi
