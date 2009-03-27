bin_PROGRAMS = html2xhtml dtdquery
html2xhtml_SOURCES = dtd.c dtd_names.c dtd_util.c htmlgr.y html.l html2xhtml.c \
	mensajes.c procesador.c tree.c xchar.c charset.c cgi.c params.c \
	charset_aliases.c
dtdquery_SOURCES = dtdquery.c tree.c dtd.c dtd_names.c dtd_util.c xchar.c \
	mensajes.c

# set the include path found by configure
INCLUDES= $(all_includes)

# the library search path.
html2xhtml_LDFLAGS = $(all_libraries) 
html2xhtml_LDADD = @LIBICONV@
noinst_HEADERS = dtd.h dtd_names.h dtd_types.h dtd_util.h \
	mensajes.h procesador.h tree.h xchar.h htmlgr.h charset.h cgi.h \
	params.h charset.h charset_aliases.h

AM_YFLAGS = -d

## My version of Flex in MinGW only reads the .l file from stdin
##.l.c:
##	$(LEX) $(AM_LFLAGS) $(LFLAGS) < $< && mv $(LEX_OUTPUT_ROOT).c $@
