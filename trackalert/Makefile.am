AM_CPPFLAGS = $(LUA_CFLAGS) $(YAHTTP_CFLAGS) $(libsodium_CFLAGS) $(JSON11_CFLAGS) $(LIBHIREDIS_CFLAGS) $(LIBCURL_CFLAGS) $(LIBCRYPTO_INCLUDES) $(YAMLCPP_FLAGS) -O3 -Wall -pthread -DSYSCONFDIR='"$(sysconfdir)"'
ACLOCAL_AMFLAGS = -I m4

SUBDIRS=ext

EXTRA_DIST= trackalert.conf README.md trackalert.service.in

CLEANFILES = trackalert.service
distclean-local:
	-rm -rf *.dSYM

sysconf_DATA= trackalert.conf 
bin_PROGRAMS = trackalert
trackalert_SOURCES = \
	base64.hh \
	trackalert.cc trackalert.hh \
	trackalert-lua.cc \
	trackalert-web.cc trackalert-web.hh\
	dolog.hh wforce_ns.hh \
	iputils.cc iputils.hh \
	misc.cc misc.hh \
	namespaces.hh \
	perf-stats.cc perf-stats.hh \
	webhook.cc webhook.hh \
	minicurl.cc minicurl.hh \
	twmap.hh twmap.cc \
	hmac.hh hmac.cc \
	wforce_exception.hh \
	sholder.hh \
	sodcrypto.cc sodcrypto.hh \
	trackalert-luastate.hh \
	lock.hh \
	pdnsexception.hh \
	device_parser.hh device_parser.cc \
	wforce-webserver.cc wforce-webserver.hh \
	sstuff.hh ext/luawrapper/include/LuaContext.hpp \
	login_tuple.cc login_tuple.hh \
	wforce-geoip.cc wforce-geoip.hh \
	ext/incbin/incbin.h \
	ext/murmur3.cc \
        ext/murmur3.h \
	ext/count_min_sketch.cpp \
	ext/ctpl.h \
	ext/hyperloglog.hpp \
	ext/count_min_sketch.hpp \
	ext/uap-cpp/UaParser.h \
	ext/uap-cpp/UaParser.cpp

trackalert_LDFLAGS = \
	$(AM_LDFLAGS) \
	-pthread 

trackalert_LDADD = \
	-lreadline -ltermcap \
	$(LUA_LIBS) $(YAHTTP_LIBS) $(libsodium_LIBS) \
	$(LIBSYSTEMD_LIBS) $(JSON11_LIBS) $(BOOST_DATE_TIME_LIBS) \
	$(BOOST_REGEX_LIBS) $(LIBHIREDIS_LIBS) $(LIBCURL) $(LIBCRYPTO_LIBS) \
	$(YAMLCPP_LIBS) $(GEOIP_LIBS)

noinst_HEADERS = \
	base64.hh \
	dolog.hh \
	iputils.hh \
	misc.hh \
	namespaces.hh \
	sholder.hh \
	sodcrypto.hh \
	sstuff.hh \
	trackalert.hh \
	perf-stats.hh \
	webhook.hh \
	minicurl.hh \
	twmap.hh \
	hmac.hh \
	wforce_exception.hh \
	wforce_ns.hh \
	trackalert-luastate.hh \
	device_parser.hh \
	lock.hh \
	pdnsexception.hh \
	wforce-webserver.hh \
	trackalert-web.hh \
	login_tuple.hh \
	wforce-geoip.hh

if HAVE_SYSTEMD
SYSTEMD_TYPE = notify
systemdsystemunitdir = $(SYSTEMD_DIR)
systemdsystemunit_DATA = trackalert.service
else
SYSTEMD_TYPE = simple
endif
trackalert.service: trackalert.service.in
	$(AM_V_GEN)sed -e 's![@]bindir[@]!$(bindir)!' -e 's![@]type[@]!$(SYSTEMD_TYPE)!' < $< > $@
