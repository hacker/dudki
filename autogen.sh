#!/bin/sh
WANT_AUTOMAKE=1.8
export WANT_AUTOMAKE
WANT_AUTOCONF=2.5
export WANT_AUTOCONF
aclocal \
&& autoheader \
&& automake -a \
&& autoconf \
&& ./configure "$@"
