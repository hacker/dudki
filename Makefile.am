SUBDIRS = src man
EXTRA_DIST = NEWS NEWS.xml NEWS.xsl

all-local: NEWS
NEWS: NEWS.xsl NEWS.xml
	${XSLTPROC} -o $@ NEWS.xsl NEWS.xml
