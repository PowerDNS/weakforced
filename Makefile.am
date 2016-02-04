AM_CPPFLAGS = -I pdns $(LUA_CFLAGS) $(YAHTTP_CFLAGS) $(libsodium_CFLAGS) $(GETDNS_CFLAGS) -O3 -Wall -pthread -DSYSCONFDIR='"$(sysconfdir)"'
ACLOCAL_AMFLAGS = -I m4

SUBDIRS=pdns/ext/yahttp docs

BUILT_SOURCES=htmlfiles.h

htmlfiles.h: html/*
	./incfiles > $@

EXTRA_DIST= wforce.conf README.md html wforce.service

sysconf_DATA= wforce.conf wforce.conf.example
bin_PROGRAMS = wforce
wforce_SOURCES = \
	base64.hh \
	dns_lookup.cc \
	wforce.cc wforce.hh \
	wforce-lua.cc \
	wforce-web.cc \
	wforce-geoip.cc \
	twmap.cc \
	dolog.hh \
	iputils.cc iputils.hh \
	misc.cc misc.hh \
	htmlfiles.h \
	namespaces.hh \
	sholder.hh \
	sodcrypto.cc sodcrypto.hh \
	sstuff.hh pdns/ext/luawrapper/include/LuaContext.hpp \
	pdns/ext/json11/json11.cpp \
	pdns/ext/json11/json11.hpp \
	pdns/ext/incbin/incbin.h \
	ext/murmur3.cc \
	ext/count_min_sketch.cpp

wforce_LDFLAGS = \
	$(AM_LDFLAGS) \
	-pthread 

wforce_LDADD = \
	-lreadline -ltermcap -lboost_serialization \
	$(LUA_LIBS) $(YAHTTP_LIBS) ${libsodium_LIBS} $(GEOIP_LIBS) $(GETDNS_LIBS) $(LIBSYSTEMD_LIBS)

if WITH_SYSTEMDSYSTEMUNITDIR
wforce.service: wforce.service.in
	$(AM_V_GEN)sed -e 's![@]bindir[@]!$(bindir)!' < $< > $@

systemdsystemunit_DATA = wforce.service
endif
