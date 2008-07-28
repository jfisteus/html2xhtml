#!/bin/sh

REFDIR=reference
ODIR=tmp-test
FAIL_FILE=fails

if [ $# -eq 1 ]
then
    echo "***** diff $REFDIR/$1.ref $ODIR/$1.out"
    diff $REFDIR/$1.ref $ODIR/$1.out 
else
    if [ -f $FAIL_FAIL ]
    then
	for f in `cat $FAIL_FILE`
	do
	    echo "***** diff $REFDIR/$f.ref $ODIR/$f.out"
	    diff $REFDIR/$f.ref $ODIR/$f.out
	done
    fi
fi
