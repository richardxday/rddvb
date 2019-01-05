#!/bin/sh
HOSTNAME=`hostname`
make -j4 && make install && git commit -a && git push
for HOST in abacus pivr richard-thinkpad ; do
	if [ "$HOSTNAME" != "$HOST" ] ; then
		echo "Connecting to $HOST (from $HOSTNAME)..."
		ssh -t $HOST "cd code/rddvb ; git fetch && git rebase && make -j4 && make install"
	fi
done
