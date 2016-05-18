AM_CPPFLAGS = -I pdns $(LUA_CFLAGS) $(YAHTTP_CFLAGS) $(libsodium_CFLAGS) $(GETDNS_CFLAGS) $(JSON11_CFLAGS) -O3 -Wall -pthread -DSYSCONFDIR='"$(sysconfdir)"'
ACLOCAL_AMFLAGS = -I m4

SUBDIRS=ext docs

EXTRA_DIST= wforce.conf README.md wforce.service.in wforce.conf.example

sysconf_DATA= wforce.conf wforce.conf.example
bin_PROGRAMS = wforce
wforce_SOURCES = \
	base64.hh \
	dns_lookup.cc \
	wforce.cc wforce.hh \
	wforce-lua.cc \
	wforce-web.cc \
	wforce-geoip.cc \
	twmap.cc twmap.hh \
	blacklist.cc blacklist.hh \
	dolog.hh \
	iputils.cc iputils.hh \
	misc.cc misc.hh \
	namespaces.hh \
	sholder.hh \
	sodcrypto.cc sodcrypto.hh \
	sstuff.hh ext/luawrapper/include/LuaContext.hpp \
	ext/incbin/incbin.h \
	ext/incbin/incbin.h \
	ext/murmur3.cc \
        ext/murmur3.h \
	ext/count_min_sketch.cpp \
	ext/ctpl.h \
	ext/hyperloglog.hpp \
	ext/count_min_sketch.hpp


wforce_LDFLAGS = \
	$(AM_LDFLAGS) \
	-pthread 

wforce_LDADD = \
	-lreadline -ltermcap \
	$(LUA_LIBS) $(YAHTTP_LIBS) ${libsodium_LIBS} $(GEOIP_LIBS) $(GETDNS_LIBS) $(LIBSYSTEMD_LIBS) $(JSON11_LIBS) $(BOOST_DATE_TIME_LIBS)

noinst_HEADERS = \
	base64.hh \
	dns.hh \
	dns_lookup.hh \
	dolog.hh \
	iputils.hh \
	misc.hh \
	namespaces.hh \
	sholder.hh \
	sodcrypto.hh \
	sstuff.hh \
	twmap.hh \
	blacklist.hh \
	wforce-geoip.hh \
	wforce.hh

if HAVE_SYSTEMD
SYSTEMD_TYPE = notify
systemdsystemunitdir = $(SYSTEMD_DIR)
systemdsystemunit_DATA = wforce.service
else
SYSTEMD_TYPE = simple
endif
wforce.service: wforce.service.in
	$(AM_V_GEN)sed -e 's![@]bindir[@]!$(bindir)!' -e 's![@]type[@]!$(SYSTEMD_TYPE)!' < $< > $@
