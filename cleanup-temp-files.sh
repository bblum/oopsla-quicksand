#!/bin/sh

if [ ! -d "ls" -o ! -d "qs" ]; then
	echo "cleanup script run from wrong directory; i don't see ls/ or qs/"
	exit 1
fi

rm -f qs/ls-output.log.*
rm -f qs/ls-setup.log.*
rm -f ls/config.quicksand.*
rm -f ls/pps-and-such.quicksand.*
rm -f landslide-trace-*.html
