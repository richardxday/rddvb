#!/bin/sh
DATA="/var/dvb"
CONFIG="/etc/dvb"
DEST="/home/richard/Dropbox/Personal/dvb"
COUNT=14

INDEX=$(((`date +%s`/3600/24)%$COUNT))
FILE="$DEST/dvbdata.$INDEX.7z"
if [ -f "$FILE" ] ; then
  rm "$FILE"
fi
7z a "$FILE" $DATA/recorded.dat $DATA/episodes.txt $CONFIG/*
