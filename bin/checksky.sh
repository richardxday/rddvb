#!/bin/sh
cd ~/programmes-of-interest
if dvb -r /etc/dvb/data/skylistings.dat --find-with-file patterns.txt -w foundprogrammes.dat ; then
  echo "Summary:" >newlist.txt
  dvb -r foundprogrammes.dat -v --list-to-file newlist.txt >/dev/null
  echo "" >>newlist.txt
  echo "Details:" >>newlist.txt
  dvb -r foundprogrammes.dat -v10 --list-to-file newlist.txt >/dev/null
  if ! diff newlist.txt list.txt >/dev/null ; then
    cp newlist.txt list.txt
    echo "Sending email"
    mail -s "DVB Programmes of Interest" r.day80@gmail.com <list.txt
  fi
  rm newlist.txt
fi

