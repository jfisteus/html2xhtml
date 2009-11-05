*** Information regarding the html2xhtml Windows binary package ***

Html2xhtml is a command-line program. In Windows systems, it can be
run from a command-line interface. For example, the following command
converts the input file "test.html" and writes the output to
"test-converted.html":

html2xhtml test.html -o test-converted.html

Documentation about the command-line options that html2xhtml accepts
can be found in the file doc/html2xhtml.txt distributed with the
program.

The files html2xhtml.exe and libiconv-2.dll distributed in the Windows
binary package must be either in the same directory from which the
command is run or in a directory that is in the system PATH variable.
The easiest way to install it in your Windows system is to create a
new folder for it (for example, "C:\Program Files\html2xhtml"), put both
html2xhtml.exe and libiconv-2.dll inside, and append
"C:\Program Files\html2xhtml" to the system PATH variable.

Volunteers to develop a graphical user interface and/or a installer
for Windows systems are welcome ;)

These binary files html2xhtml.exe and libiconv-2.dll have been tested
on a 32-bit Windows XP machine.

libiconv-2.dll is part of the project libiconv and is licensed under
the LGPL. Html2xhtml uses it to handle different character sets. More
detailed information, including its source code, can be obtained from:

http://www.gnu.org/software/libiconv/
