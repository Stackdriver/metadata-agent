# SUBDIRS += daemon

# AM_CPPFLAGS = -I$(srcdir)/daemon
AM_CPPFLAGS += -DPREFIX='"${prefix}"'
AM_CPPFLAGS += -DCONFIGFILE='"${sysconfdir}/${PACKAGE_NAME}.conf"'
AM_CPPFLAGS += -DLOCALSTATEDIR='"${localstatedir}"'
AM_CPPFLAGS += -DPKGLOCALSTATEDIR='"${localstatedir}/lib/${PACKAGE_NAME}"'
AM_CPPFLAGS += -DPIDFILE='"${localstatedir}/run/${PACKAGE_NAME}.pid"'
AM_CPPFLAGS += -DPKGDATADIR='"${pkgdatadir}"'

LOG_COMPILER = env VALGRIND="@VALGRIND@" $(abs_top_srcdir)/testwrapper.sh

V_PROTOC = $(v_protoc_@AM_V@)
v_protoc_ = $(v_protoc_@AM_DEFAULT_V@)
v_protoc_0 = @echo "  PROTOC  " $@;

noinst_LTLIBRARIES =
check_PROGRAMS =
TESTS =

noinst_LTLIBRARIES += libformat_json.la
libformat_json_la_SOURCES   = utils_format_json.c utils_format_json.h
libformat_json_la_CPPFLAGS  = $(AM_CPPFLAGS)
libformat_json_la_LDFLAGS   = $(AM_LDFLAGS)
libformat_json_la_LIBADD    =
if BUILD_WITH_LIBYAJL
libformat_json_la_CPPFLAGS += $(BUILD_WITH_LIBYAJL_CPPFLAGS)
libformat_json_la_LDFLAGS  += $(BUILD_WITH_LIBYAJL_LDFLAGS)
libformat_json_la_LIBADD   += $(BUILD_WITH_LIBYAJL_LIBS)
check_PROGRAMS += test_format_json
TESTS += test_format_json
test_format_json_SOURCES = utils_format_json_test.c testing.h
test_format_json_LDADD = libformat_json.la daemon/libmetadata.la daemon/libplugin_mock.la -lm
endif

noinst_LTLIBRARIES += liblatency.la
liblatency_la_SOURCES = utils_latency.c utils_latency.h
check_PROGRAMS += test_utils_latency
TESTS += test_utils_latency
test_utils_latency_SOURCES = utils_latency_test.c testing.h
test_utils_latency_LDADD = liblatency.la daemon/libplugin_mock.la -lm

noinst_LTLIBRARIES += liblookup.la
liblookup_la_SOURCES = utils_vl_lookup.c utils_vl_lookup.h
liblookup_la_LIBADD = daemon/libavltree.la
check_PROGRAMS += test_utils_vl_lookup
TESTS += test_utils_vl_lookup
test_utils_vl_lookup_SOURCES = utils_vl_lookup_test.c testing.h
test_utils_vl_lookup_LDADD = liblookup.la daemon/libplugin_mock.la
if BUILD_WITH_LIBKSTAT
test_utils_vl_lookup_LDADD += -lkstat
endif

noinst_LTLIBRARIES += libmount.la
libmount_la_SOURCES = utils_mount.c utils_mount.h
check_PROGRAMS += test_utils_mount
TESTS += test_utils_mount
test_utils_mount_SOURCES = utils_mount_test.c testing.h
test_utils_mount_LDADD = libmount.la daemon/libplugin_mock.la
if BUILD_WITH_LIBKSTAT
test_utils_mount_LDADD += -lkstat
endif

sbin_PROGRAMS = collectdmon
bin_PROGRAMS = collectd-nagios collectdctl collectd-tg

collectdmon_SOURCES = collectdmon.c

collectd_nagios_SOURCES = collectd-nagios.c
collectd_nagios_CPPFLAGS = $(AM_CPPFLAGS) \
	-I$(top_srcdir)/src/libcollectdclient/collectd \
	-I$(top_builddir)/src/libcollectdclient/collectd
collectd_nagios_LDADD = libcollectdclient/libcollectdclient.la
if BUILD_WITH_LIBSOCKET
collectd_nagios_LDADD += -lsocket
endif
if BUILD_AIX
collectd_nagios_LDADD += -lm
endif


collectdctl_SOURCES = collectdctl.c
collectdctl_CPPFLAGS = $(AM_CPPFLAGS) \
	-I$(top_srcdir)/src/libcollectdclient/collectd \
	-I$(top_builddir)/src/libcollectdclient/collectd
collectdctl_LDADD = libcollectdclient/libcollectdclient.la
if BUILD_WITH_LIBSOCKET
collectdctl_LDADD += -lsocket
endif
if BUILD_AIX
collectdctl_LDADD += -lm
endif


collectd_tg_SOURCES = collectd-tg.c
collectd_tg_CPPFLAGS = $(AM_CPPFLAGS) \
	-I$(top_srcdir)/src/libcollectdclient/collectd \
	-I$(top_builddir)/src/libcollectdclient/collectd
collectd_tg_LDADD = \
	$(PTHREAD_LIBS) \
	daemon/libheap.la \
	libcollectdclient/libcollectdclient.la
if BUILD_WITH_LIBSOCKET
collectd_tg_LDADD += -lsocket
endif
if BUILD_WITH_LIBRT
collectd_tg_LDADD += -lrt
endif
if BUILD_AIX
collectd_tg_LDADD += -lm
endif


pkglib_LTLIBRARIES =

BUILT_SOURCES =
CLEANFILES =

if HAVE_PROTOC3
if HAVE_GRPC_CPP
BUILT_SOURCES += collectd.grpc.pb.cc collectd.pb.cc types.pb.cc
CLEANFILES += collectd.grpc.pb.cc collectd.pb.cc types.pb.cc \
		collectd.grpc.pb.h collectd.pb.h types.pb.h

collectd.grpc.pb.cc: $(top_srcdir)/proto/collectd.proto $(top_srcdir)/proto/types.proto
	$(V_PROTOC)@PROTOC@ -I$(top_srcdir)/proto \
		--grpc_out=$(builddir) --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN) $<

collectd.pb.cc: $(top_srcdir)/proto/collectd.proto $(top_srcdir)/proto/types.proto
	$(V_PROTOC)@PROTOC@ -I$(top_srcdir)/proto --cpp_out=$(builddir) $<

types.pb.cc: $(top_srcdir)/proto/types.proto
	$(V_PROTOC)@PROTOC@ -I$(top_srcdir)/proto --cpp_out=$(builddir) $<
endif
endif

if BUILD_PLUGIN_CURL
pkglib_LTLIBRARIES += curl.la
curl_la_SOURCES = curl.c \
		  utils_curl_stats.c utils_curl_stats.h
curl_la_LDFLAGS = $(PLUGIN_LDFLAGS)
curl_la_CFLAGS = $(AM_CFLAGS) $(BUILD_WITH_LIBCURL_CFLAGS)
curl_la_LIBADD = $(BUILD_WITH_LIBCURL_LIBS)
endif

if BUILD_PLUGIN_CURL_JSON
pkglib_LTLIBRARIES += curl_json.la
curl_json_la_SOURCES = curl_json.c \
		  utils_curl_stats.c utils_curl_stats.h
curl_json_la_CFLAGS = $(AM_CFLAGS) $(BUILD_WITH_LIBCURL_CFLAGS)
curl_json_la_CPPFLAGS = $(AM_CPPFLAGS) $(BUILD_WITH_LIBYAJL_CPPFLAGS)
curl_json_la_LDFLAGS = $(PLUGIN_LDFLAGS) $(BUILD_WITH_LIBYAJL_LDFLAGS)
curl_json_la_LIBADD = $(BUILD_WITH_LIBCURL_LIBS) $(BUILD_WITH_LIBYAJL_LIBS)
endif

if BUILD_PLUGIN_GRPC
pkglib_LTLIBRARIES += grpc.la
grpc_la_SOURCES = grpc.cc
nodist_grpc_la_SOURCES = collectd.grpc.pb.cc collectd.pb.cc types.pb.cc
grpc_la_CPPFLAGS = $(AM_CPPFLAGS) $(BUILD_WITH_LIBGRPCPP_CPPFLAGS) $(BUILD_WITH_LIBPROTOBUF_CPPFLAGS)
grpc_la_LDFLAGS = $(PLUGIN_LDFLAGS) $(BUILD_WITH_LIBGRPCPP_LDFLAGS) $(BUILD_WITH_LIBPROTOBUF_LDFLAGS)
grpc_la_LIBADD = $(BUILD_WITH_LIBGRPCPP_LIBS) $(BUILD_WITH_LIBPROTOBUF_LIBS)
endif

if BUILD_PLUGIN_PERL
pkglib_LTLIBRARIES += perl.la
perl_la_SOURCES = perl.c
# Despite C99 providing the "bool" type thru stdbool.h, Perl defines its own
# version of that type if HAS_BOOL is not defined... *sigh*
perl_la_CPPFLAGS = $(AM_CPPFLAGS) -DHAS_BOOL=1
# Despite off_t being 64 bit wide on 64 bit platforms, Perl insist on using
# off64_t which is only exposed when _LARGEFILE64_SOURCE is defined... *sigh*
# On older platforms we also need _REENTRANT. _GNU_SOURCE sets both of these.
perl_la_CPPFLAGS += -D_GNU_SOURCE
perl_la_CFLAGS  = $(AM_CFLAGS) \
		$(PERL_CFLAGS) \
		-DXS_VERSION=\"$(VERSION)\" -DVERSION=\"$(VERSION)\"
# Work-around for issues #41 and #42 - Perl 5.10 incorrectly introduced
# __attribute__nonnull__(3) for Perl_load_module().
if HAVE_BROKEN_PERL_LOAD_MODULE
perl_la_CFLAGS += -Wno-nonnull
endif
perl_la_LDFLAGS = $(PLUGIN_LDFLAGS) \
		$(PERL_LDFLAGS)
perl_la_LIBADD = $(PERL_LIBS)
endif

if BUILD_PLUGIN_PYTHON
pkglib_LTLIBRARIES += python.la
python_la_SOURCES = python.c pyconfig.c pyvalues.c cpython.h
python_la_CPPFLAGS = $(AM_CPPFLAGS) $(LIBPYTHON_CPPFLAGS)
if COMPILER_IS_GCC
python_la_CPPFLAGS += -fno-strict-aliasing -Wno-strict-aliasing
endif
python_la_LDFLAGS = $(PLUGIN_LDFLAGS) $(LIBPYTHON_LDFLAGS)
endif

if BUILD_PLUGIN_WRITE_GCM
pkglib_LTLIBRARIES += write_gcm.la
write_gcm_la_SOURCES = write_gcm.c
write_gcm_la_LDFLAGS = -module -avoid-version $(BUILD_WITH_LIBYAJL_LDFLAGS)
write_gcm_la_CFLAGS = $(AM_CFLAGS) $(BUILD_WITH_LIBYAJL_CPPFLAGS)
write_gcm_la_LIBADD = $(BUILD_WITH_LIBYAJL_LIBS)
if BUILD_WITH_LIBSSL
write_gcm_la_CFLAGS += $(BUILD_WITH_LIBSSL_CFLAGS)
write_gcm_la_LIBADD += $(BUILD_WITH_LIBSSL_LIBS)
endif
if BUILD_WITH_LIBCURL
write_gcm_la_CFLAGS += $(BUILD_WITH_LIBCURL_CFLAGS)
write_gcm_la_LIBADD += $(BUILD_WITH_LIBCURL_LIBS)
endif
endif

BUILT_SOURCES += $(dist_man_MANS)

if BUILD_PLUGIN_ZONE
pkglib_LTLIBRARIES += zone.la
zone_la_SOURCES = zone.c
zone_la_LDFLAGS = $(PLUGIN_LDFLAGS)
endif

dist_man_MANS = collectd.1 \
		collectd.conf.5 \
		collectd-email.5 \
		collectd-exec.5 \
		collectdctl.1 \
		collectd-java.5 \
		collectd-lua.5 \
		collectdmon.1 \
		collectd-nagios.1 \
		collectd-perl.5 \
		collectd-python.5 \
		collectd-snmp.5 \
		collectd-tg.1 \
		collectd-threshold.5 \
		collectd-unixsock.5 \
		types.db.5

EXTRA_DIST =	collectd.conf.pod \
		collectd-email.pod \
		collectd-exec.pod \
		collectdctl.pod \
		collectd-java.pod \
		collectd-lua.pod \
		collectdmon.pod \
		collectd-nagios.pod \
		collectd-perl.pod \
		collectd-python.pod \
		collectd.pod \
		collectd-snmp.pod \
		collectd-tg.pod \
		collectd-threshold.pod \
		collectd-unixsock.pod \
		postgresql_default.conf \
		types.db \
		types.db.pod \
		valgrind.FreeBSD.suppress

AM_V_POD2MAN_C = $(am__v_POD2MAN_C_@AM_V@)
am__v_POD2MAN_C_ = $(am__v_POD2MAN_C_@AM_DEFAULT_V@)
am__v_POD2MAN_C_0 = @echo "  POD2MAN " $@;
am__v_POD2MAN_C_1 =

.pod.1:
	$(AM_V_POD2MAN_C)pod2man --release=$(VERSION) --center=$(PACKAGE) $< \
		>.pod2man.tmp.$$$$ 2>/dev/null && mv -f .pod2man.tmp.$$$$ $@ || true
	@if grep '\<POD ERRORS\>' $@ >/dev/null 2>&1; \
	then \
		echo "$@ has some POD errors!"; false; \
	fi

.pod.5:
	$(AM_V_POD2MAN_C)pod2man --section=5 --release=$(VERSION) --center=$(PACKAGE) $< \
		>.pod2man.tmp.$$$$ 2>/dev/null && mv -f .pod2man.tmp.$$$$ $@ || true
	@if grep '\<POD ERRORS\>' $@ >/dev/null 2>&1; \
	then \
		echo "$@ has some POD errors!"; false; \
	fi

AM_V_PROTOC_C = $(am__v_PROTOC_C_@AM_V@)
am__v_PROTOC_C_ = $(am__v_PROTOC_C_@AM_DEFAULT_V@)
am__v_PROTOC_C_0 = @echo "  PROTOC-C" $@;
am__v_PROTOC_C_1 =

install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)
	if test -e $(DESTDIR)$(sysconfdir)/collectd.conf; \
	then \
		$(INSTALL) -m 0640 collectd.conf $(DESTDIR)$(sysconfdir)/collectd.conf.pkg-orig; \
	else \
		$(INSTALL) -m 0640 collectd.conf $(DESTDIR)$(sysconfdir)/collectd.conf; \
	fi; \
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)
	$(INSTALL) -m 0644 $(srcdir)/types.db $(DESTDIR)$(pkgdatadir)/types.db;
	$(INSTALL) -m 0644 $(srcdir)/postgresql_default.conf \
		$(DESTDIR)$(pkgdatadir)/postgresql_default.conf;

uninstall-hook:
	rm -f $(DESTDIR)$(pkgdatadir)/types.db;
	rm -f $(DESTDIR)$(sysconfdir)/collectd.conf
	rm -f $(DESTDIR)$(pkgdatadir)/postgresql_default.conf;
