# # Don't want this to be done automatically, in order to avoid
# # the dependency with python and Web access.
# # The specific rule charsets is defined instead.
# # type "make charsets" to build charset_aliases.h/.c

GENERATOR = ./charsets_gen.py
DEST_DIR = ../src
EXTRA_DIST = $(GENERATOR)

CLEANFILES = iana_charsets iconv_charsets

iana_charsets:
	curl http://www.iana.org/assignments/character-sets -o iana_charsets

iconv_charsets:
	iconv -l >iconv_charsets

charsets: iana_charsets iconv_charsets
	$(GENERATOR) iana_charsets iconv_charsets $(DEST_DIR)


