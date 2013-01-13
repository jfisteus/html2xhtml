#!/bin/sh

if [ ! $# -eq 1 ]
then
    echo "Input HTML file name is expected as a command-line parameter" >&2
else
    curl --data-binary @$1 -H "Content-Type: text/html"  http://www.it.uc3m.es/jaf/cgi-bin/html2xhtml.cgi
fi
