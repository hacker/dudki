AC_INIT([dudki], [0.1], [dudki-bugs@klever.net])
AC_CONFIG_SRCDIR([src/dudki.cc])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([dist-bzip2])

AC_PROG_CXX
AC_PROG_CC

AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([syslog.h unistd.h getopt.h])

AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_PID_T

AC_FUNC_FORK
AC_HEADER_STDC
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([dup2 memmove strtol])
AC_CHECK_FUNC([getopt_long],[
 AC_DEFINE([HAVE_GETOPT_LONG],[1],[Define to make use of getopt_long])
 AC_SUBST([HAVE_GETOPT_LONG],1)
],[
 AC_SUBST([HAVE_GETOPT_LONG],0)
])

AC_PATH_PROG([XSLTPROC],[xsltproc],[true])

PKG_CHECK_MODULES([DOTCONF],[dotconf],,[
 AC_MSG_ERROR([no dotconf library found])
])

AC_CONFIG_FILES([
 Makefile
 src/Makefile
 man/Makefile
  man/dudki.8 man/dudki.conf.5
])
AC_OUTPUT
