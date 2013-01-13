#!/usr/bin/python

#
# Example client script for the html2xhtml service.
#

import sys
import httplib, urllib

headers = {"Content-type": "text/html",
           "Accept": "application/xhtml+xml"}
params = urllib.urlencode({'tablength': 4,
                           'linelength': 100,
                           'output-charset': 'UTF-8'})
url = "/jaf/cgi-bin/html2xhtml.cgi?" + params

# read the input HTML file (first command-line argument)
in_file = open(sys.argv[1], 'r')
in_data = in_file.read()

# connect to the server and send the POST request
conn = httplib.HTTPConnection("www.it.uc3m.es:80")
conn.request("POST", url, in_data, headers)
response = conn.getresponse()

# show the result
if response.status == 200:
    print response.read()
else:
    print >> sys.stderr, response.status, response.reason

conn.close()
