sbin_PROGRAMS = dudki

INCLUDES = ${DOTCONF_CFLAGS}
LIBS += ${DOTCONF_LIBS}
AM_CPPFLAGS = \
	      -DDEFAULT_CONF_FILE=\"${sysconfdir}/${PACKAGE}.conf\" \
	      -DDEFAULT_PID_FILE=\"/var/run/${PACKAGE}.pid\"

dudki_SOURCES = dudki.cc \
	process.cc process.h \
	configuration.cc configuration.h \
	util.cc util.h \
	COPYING.cc

COPYING.cc: ${top_srcdir}/COPYING
	echo "const char * COPYING =" >$@ || (rm $@;exit 1)
	sed 's/"/\\"/g' $< | sed 's/^/\"/' | sed 's/$$/\\n\"/' >>$@ || (rm $@;exit 1)
	echo ";" >>$@ || (rm $@;exit 1)
