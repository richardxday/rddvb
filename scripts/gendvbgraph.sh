#!/bin/sh
TIME=$1
DIR="`dvb --datadir`/graphs"
FILENAME="graph-`date +%Y-%m-%d`.png"

mkdir -p "$DIR"
cd "$DIR"

test -n "$TIME" || TIME=6
STARTDATE="`date --date="-$TIME month" +%d-%b-%Y`"

dvb -r recorded --calc-trend "`date --date="-6 month" +%d-%b-%Y`" | tee trend.txt

TREND="`grep "Trend:" trend.txt | sed -E "s/Trend: //"`"
PERDAY="`grep "Average:" trend.txt | sed -E "s/^.+ ([0-9\.]+) .+$/\1/"`"

dvb -r recorded --list >list.dat && cat <<EOF >plot.gnp && gnuplot plot.gnp
set terminal pngcairo size 1280,800
set output "$FILENAME"
set xdata time
set timefmt '%d-%b-%Y %H:%M'
set autoscale xy
set grid
set xtics rotate by -90 format "%d-%b-%Y"
set xrange ["$STARTDATE":*]
plot \
'list.dat' using 2:0 with lines title 'Recorded', \
$TREND with lines title '$PERDAY Recordings/day'
unset output
set output "graph.png"
replot
unset output
set terminal pngcairo size 640,480
set output "graph-preview.png"
set xrange ["`date --date='-1 month' +%d-%b-%Y`":*]
replot
EOF
