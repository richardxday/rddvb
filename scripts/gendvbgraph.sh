#!/bin/sh
TIME=$1
DIR="`dvb --datadir`/graphs"
FILENAME="graph-`date +%Y-%m-%d`.png"

mkdir -p "$DIR"
cd "$DIR"

test -n "$TIME" || TIME=6
STARTDATE="`date --date="-$TIME month" +%d-%b-%Y`"
ENDDATE="`date --date="+2 weeks" +%d-%b-%Y`"
SCHSTARTDATE="`date --date="-1 day 00:00:00" +%s`"

dvb -r recorded --calc-trend "`date --date="-6 month" +%d-%b-%Y`" | tee recorded_trend.txt
dvb -r combined --calc-trend "now" >scheduled_trend.txt

RECTREND="`grep "Trend:" recorded_trend.txt | sed -E "s/Trend: //"`"
RECPERDAY="`grep "Average:" recorded_trend.txt | sed -E "s/^.+ ([0-9\.]+) .+$/\1/"`"

SCHTREND="`grep "Trend:" scheduled_trend.txt | sed -E "s/Trend: //"`"
SCHPERDAY="`grep "Average:" scheduled_trend.txt | sed -E "s/^.+ ([0-9\.]+) .+$/\1/"`"

dvb -r recorded --list >recordedlist.dat && dvb -r combined --list >combinedlist.dat && cat <<EOF >plot.gnp && gnuplot plot.gnp
set terminal pngcairo size 1280,800
set output "$FILENAME"
set xdata time
set timefmt '%d-%b-%Y %H:%M'
set autoscale xy
set grid
set xtics rotate by -90 format "%d-%b-%Y"
set xrange ["$STARTDATE":"$ENDDATE"]
set key inside bottom right
plot \\
'combinedlist.dat' using 2:0 with lines title 'Scheduled', \\
'recordedlist.dat' using 2:0 with lines title 'Recorded', \\
$RECTREND with lines title '$RECPERDAY Recorded/day', \\
(x>=$SCHSTARTDATE) ? ($SCHTREND) : 1/0 with lines title '$SCHPERDAY Scheduled/day' lt 6
unset output
set output "graph.png"
replot
unset output
set terminal pngcairo size 640,480
set output "graph-preview.png"
set xrange ["`date --date='-1 month' +%d-%b-%Y`":"$ENDDATE"]
replot
EOF
