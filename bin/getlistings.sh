#!/bin/sh
DATADIR="`dvb --datadir`"
CONFDIR="`dvb --confdir`"

cd "$DATADIR"

LISTINGSFILE="$DATADIR/archives/listings-`date +%Y-%m-%d`.xmltv"
if [ ! -f "$LISTINGSFILE.gz" ] ; then
  tv_grab_uk_rt >"$LISTINGSFILE"
  dvb --update "$LISTINGSFILE"
  gzip "$LISTINGSFILE"
fi
dvb --find-recorded-programmes-on-disk --check-recording-file
dvb --schedule

LISTINGSFILE="$DATADIR/archives/listings-sky-`date +%Y-%m-%d`.xmltv"
if [ ! -f "$LISTINGSFILE.gz" ] ; then
  tv_grab_uk_rt --config-file ~/.xmltv/tv_grab_uk_rt-sky.conf >"$LISTINGSFILE"
  dvb --read "$LISTINGSFILE" --update-dvb-channels --write "$DATADATA/skylistings.dat"
  checksky.sh
  gzip "$LISTINGSFILE"
fi
