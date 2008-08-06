#!/bin/sh

run_gdb=0

if [ "$1" == "-d" ]
then
    run_gdb=1
    shift
fi

if [ ! $# -eq 1 ]
then
    echo "One parameter expected" >&2
    echo "$0 <input_file>" >&2
    exit 1
fi

# Set environment variables
export REQUEST_METHOD=POST
export QUERY_STRING=
export CONTENT_TYPE="multipart/form-data; boundary=----------kchnF3elYElXZLka8e4OkA"
export CONTENT_LENGTH=9999

#prepare stdin
boundary="----------kchnF3elYElXZLka8e4OkA"
eol="\r\n"
TMPFILE=`mktemp` || exit 1
echo -e -n "--$boundary$eol" >>$TMPFILE
echo -e -n "Content-Disposition: form-data; name=\"type\"$eol$eol" >>$TMPFILE
echo -e -n "strict$eol" >>$TMPFILE

echo -e -n "--$boundary$eol" >>$TMPFILE
echo -e -n "Content-Disposition: form-data; name=\"linelength\"$eol$eol" >>$TMPFILE
echo -e -n "60$eol" >>$TMPFILE

echo -e -n "--$boundary$eol" >>$TMPFILE
echo -e -n "Content-Disposition: form-data; name=\"tablength\"$eol$eol" >>$TMPFILE
echo -e -n "4$eol" >>$TMPFILE

echo -e -n "--$boundary$eol" >>$TMPFILE
echo -e -n "Content-Disposition: form-data; name=\"html\"; filename=\"conversor.html\"$eol" >>$TMPFILE
echo -e -n "Content-Type: text/html$eol$eol" >>$TMPFILE
cat $1 >>$TMPFILE
echo -e -n "${eol}--$boundary$eol" >>$TMPFILE

# Run the program like if it were run from the Web server
if [ $run_gdb -eq 0 ]
then
    ../html2xhtml <$TMPFILE
    echo "[Exit status: $?]"
else
    echo "Input in $TMPFILE"
    gdb ../html2xhtml
fi

rm -f $TMPFILE
