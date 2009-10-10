# # Don't want this to be done automatically, in order to avoid
# # the dependency with Java. Specific rules are defined instead.
# #
#noinst_javadir = .
#noinst_java_JAVA = DTDCoder.java DTDDeclHandler.java
#JAVAROOT = .

DTD_SRC = DTDCoder.java DTDDeclHandler.java XHTMLEntityResolver.java
DTDLIB_DEST = ../src
DTD_TEMPLATES = template-transitional.xhtml template-strict.xhtml \
	template-frameset.xhtml template-11.xhtml \
	template-basic10.xhtml template-basic11.xhtml \
	template-mp.xhtml template-print10.xhtml 
EXTRA_DIST = $(DTD_SRC) $(DTD_TEMPLATES) dtd_index.xml

CLEANFILES = DTDCoder.class DTDDeclHandler.class AttDecl.class ElmDecl.class \
	DTDCoder\$$DTDData.class MyStringTokenizer.class

DTDCoder.class: $(DTD_SRC)
	javac $(DTD_SRC)

dtdlib: dtd_index.xml DTDCoder.class
	java DTDCoder dtd_index.xml $(DTDLIB_DEST)

