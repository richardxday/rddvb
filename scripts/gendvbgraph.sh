#!/bin/sh
DIR="`dvb --datadir`/graphs"
FILENAME="graph-`date +%Y-%m-%d`.png"

mkdir -p "$DIR"
cd "$DIR"

dvb -r recorded --list >list.dat && cat <<EOF >plot.gnp && gnuplot plot.gnp
set terminal pngcairo size 1280,800
set output "$FILENAME"
set xdata time
set timefmt '%d-%b-%Y %H:%M'
set autoscale xy
set grid
set xtics rotate by -90 format "%d-%b-%Y"
set xrange ["`date --date='-12 month' +%d-%b-%Y`":*]
plot \
'list.dat' using 2:0 with lines title 'Recorded', \
4.49*(x-1.30e9)/(3600.0*24.0) with lines title 'Averaged'
unset output
set output "graph.png"
replot
unset output
set terminal pngcairo size 640,480
set output "graph-preview.png"
set xrange ["`date --date='-1 month' +%d-%b-%Y`":*]
replot
EOF
