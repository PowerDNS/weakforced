AM_CPPFLAGS = -I pdns $(LUA_CFLAGS) $(YAHTTP_CFLAGS) -O3 -Wall -pthread 

SUBDIRS=pdns/ext/yahttp

BUILT_SOURCES=htmlfiles.h

htmlfiles.h: html/*
	./incfiles > $@

EXTRA_DIST= wforce.conf README.md html

bin_PROGRAMS = wforce
wforce_SOURCES = \
	base64.hh \
	wforce.cc wforce.hh \
	wforce-db.cc \
	wforce-lua.cc \
	wforce-web.cc \
	wforce-geoip.cc \
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
	pdns/ext/incbin/incbin.h

wforce_LDFLAGS = \
	$(AM_LDFLAGS) \
	-pthread 

wforce_LDADD = \
	-lreadline -lrt -ltermcap \
	$(LUA_LIBS) $(YAHTTP_LIBS) ${libsodium_LIBS} $(GEOIP_LIBS)

