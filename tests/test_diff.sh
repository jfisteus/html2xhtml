#!/bin/bash

REFDIR=reference
ODIR=tmp-test
FAIL_FILE=fails

if [ $# -eq 1 ]
then
    if [ -f $1 ]
    then
	pushd $REFDIR
	files=`ls $1-*.ref`
	popd
    else
	files=$1
    fi
else
    if [ -f $FAIL_FILE ]
    then
	files=`cat $FAIL_FILE`
    fi
fi

for file in $files
do
    file=${file%.ref}
    echo "***** diff $ODIR/$file.out $REFDIR/$file.ref"
    diff $ODIR/$file.out $REFDIR/$file.ref
done

