#! /bin/sh

set -x

true \
&& rm -f aclocal.m4 \
&& rm -f -r autom4te.cache \
&& rm -f compile \
&& rm -f config.guess \
&& rm -f config.log \
&& rm -f config.status \
&& rm -f config.sub \
&& rm -f configure \
&& rm -f depcomp \
&& rm -f install-sh \
&& rm -f -r libltdl \
&& rm -f libtool \
&& rm -f ltmain.sh \
&& rm -f Makefile \
&& rm -f Makefile.in \
&& rm -f missing \
&& rm -f INSTALL \
&& rm -f -r src/.deps \
&& rm -f -r src/.libs \
&& rm -f src/*.o \
&& rm -f src/*.la \
&& rm -f src/*.lo \
&& rm -f src/config.h \
&& rm -f src/config.h.in \
&& rm -f src/config.h.in~ \
&& rm -f src/Makefile \
&& rm -f src/Makefile.in \
&& rm -f src/stamp-h1 \
&& rm -f src/stamp-h1.in \
&& rm -f src/*.pb-c.c \
&& rm -f src/*.pb-c.h \
&& rm -f src/Makefile.in \
&& rm -f src/test-suite.log \
&& rm -f src/test_common* \
&& rm -f src/test_utils*
#&& rm -f src/collectd.1 \
#&& rm -f src/collectd.conf \
#&& rm -f src/collectdctl \
#&& rm -f src/collectd-tg \
#&& rm -f src/collectd-nagios \
#&& rm -f src/collectdmon

