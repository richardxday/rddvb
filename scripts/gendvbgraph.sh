#!/bin/sh
DIR="`dvb --datadir`/graphs"
FILENAME="graph-`date +%Y-%m-%d`.png"

mkdir -p "$DIR"
cd "$DIR"

dvb -r recorded --list >list.dat
cat <<EOF >plot.gnp
set terminal pngcairo
set output "$FILENAME"
set xdata time
set timefmt '%d-%b-%Y %H:%M'
set autoscale xy
set grid
set xtics rotate by -90
set xrange [1.45175e9:*]
plot \
'list.dat' using 2:0 with lines title 'Programmes', \
4.49*(x-1.30e9)/(3600.0*24.0) with lines title ''
EOF

gnuplot plot.gnp

