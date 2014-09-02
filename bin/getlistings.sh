#!/bin/sh
cd /etc/dvb

LISTINGSFILE=archive/listings-`date +%Y-%m-%d`.xmltv
if [ ! -f $LISTINGSFILE.gz ] ; then
  tv_grab_uk_rt >$LISTINGSFILE
  dvb --update $LISTINGSFILE
  gzip $LISTINGSFILE
  dvb --read-radio-listings --write radiolistings.dat --update radiolistings.dat
  dvb --find-recorded-programmes-on-disk
fi
dvb --schedule

