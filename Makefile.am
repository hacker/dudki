SUBDIRS = src man

all-local: NEWS
NEWS: NEWS.xsl NEWS.xml
	${XSLTPROC} -o $@ NEWS.xsl NEWS.xml
