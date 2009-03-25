#!/bin/sh

# Runs ../html2xhtml with a CGI environment
#
# Command line arguments:
#
# ./test_cgi.sh [-d] [-t] <input_html_file>
#
# -d: run inside gdb
# -t: direct mode instead of multipart/form-data
#

run_gdb=0
encoding=multipart
query_string=
mime=text/html

while [ $# -gt 1 ]
do
    case $1 in
	"-d") run_gdb=1 ;;
	"-t") encoding=direct ;;
        "-q") shift; query_string=$1 ;;
        "-x") mime="application/xhtml+xml" ;;
	*) echo "Error in parameters" >&2
           exit 1 ;;
    esac
    shift
done

if [ ! $# -eq 1 ]
then
    echo "One parameter expected" >&2
    echo "$0 [-d] [-t] <input_file>" >&2
    exit 1
fi

TMPFILE=`mktemp` || exit 1

if [ "$encoding" == "multipart" ]
then
    # Set environment variables
    export REQUEST_METHOD=POST
    export QUERY_STRING=$query_string
    export CONTENT_TYPE="multipart/form-data; boundary=----------kchnF3elYElXZLka8e4OkA"
    export CONTENT_LENGTH=9999

    #prepare stdin
    boundary="----------kchnF3elYElXZLka8e4OkA"
    eol="\r\n"
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
else
    export REQUEST_METHOD=POST
    export QUERY_STRING=$query_string
    export CONTENT_TYPE=$mime
    export CONTENT_LENGTH=9999
    cat $1 >$TMPFILE
fi

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
