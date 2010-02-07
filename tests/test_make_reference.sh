#!/bin/bash

ODIR=reference
H2X=../src/html2xhtml

keys=`$H2X -L`

./test_clean.sh
mkdir $ODIR

for file in `find -name "*.html"`
do
    for key in $keys
    do
	if ! $H2X -t $key $file -o $ODIR/${file}-${key}.ref 2>/dev/null
	then
	    echo "$key" >>$ODIR/${file}-${key}.fail
	fi
    done
done

tar czf $ODIR.tar.gz $ODIR
./test_clean.sh
