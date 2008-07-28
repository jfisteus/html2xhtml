#!/bin/sh

REFDIR=reference
ODIR=tmp-test
H2X=../src/html2xhtml
FAIL_FILE=fails
FAILS=

keys=`$H2X -L`

./test_clean.sh
tar xzf $REFDIR.tar.gz

mkdir $ODIR

for file in `find -name "*.html"`
do
    for key in $keys
    do
	grep $REFDIR/${file}-${key}.fail -e $key >/dev/null 2>/dev/null
	success_expected=$?
	if ! $H2X -t $key $file -o $ODIR/${file}-${key}.out 2>/dev/null
	then
	    if [ ! $success_expected -eq 0 ]
	    then
		echo "FAIL: ${file} / ${key} - html2xhtml failed to write output"
		FAILS="$FAILS ${file}-${key}"
	    else
		echo "-OK-: ${file} / ${key}"
	    fi
	else
	   if [ $success_expected -eq 0 ]
	   then
	       echo "FAIL: ${file} / ${key} - html2xhtml expected to fail"
	       FAILS="$FAILS ${file}-${key}"
	   else
	       if diff $REFDIR/${file}-${key}.ref $ODIR/${file}-${key}.out >/dev/null
	       then
		   echo "-OK-: ${file} / ${key}"
	       else
		   echo "FAIL: ${file} / ${key} - different output"
		   FAILS="$FAILS ${file}-${key}"
	       fi
	   fi
	fi
    done
done

rm -f $FAIL_FILE

if [ ! "$FAILS" == "" ]
then
    echo "Some tests failed:"
    for fail in $FAILS
    do
	echo "$fail"
	echo "$fail" >>$FAIL_FILE
    done
    exit 1
fi
