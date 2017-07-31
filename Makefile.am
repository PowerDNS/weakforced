AM_CPPFLAGS = -I pdns $(LUA_CFLAGS) $(YAHTTP_CFLAGS) $(libsodium_CFLAGS) $(GETDNS_CFLAGS) $(JSON11_CFLAGS) $(LIBHIREDIS_CFLAGS) $(LIBCURL_CFLAGS) $(LIBCRYPTO_INCLUDES) -O3 -Wall -pthread -DSYSCONFDIR='"$(sysconfdir)"'
ACLOCAL_AMFLAGS = -I m4

SUBDIRS=ext docs

EXTRA_DIST= wforce.conf README.md wforce.service.in wforce.conf.example replication.proto

CLEANFILES = replication.pb.cc replication.pb.h wforce.service

sysconf_DATA= wforce.conf wforce.conf.example
bin_PROGRAMS = wforce
wforce_SOURCES = \
	base64.hh \
	dns_lookup.cc dns_lookup.hh \
	wforce.cc wforce.hh \
	wforce-lua.cc \
	wforce-web.cc \
	wforce-geoip.cc wforce-geoip.hh \
	twmap.cc twmap.hh \
	twmap-wrapper.cc twmap-wrapper.hh \
	blacklist.cc blacklist.hh \
	dolog.hh wforce_ns.hh \
	iputils.cc iputils.hh \
	misc.cc misc.hh \
	namespaces.hh \
	perf-stats.cc perf-stats.hh \
	replication.cc replication.hh \
	replication_sdb.hh replication_sdb.cc \
	replication_bl.cc replication_bl.hh \
	webhook.cc webhook.hh \
	minicurl.cc minicurl.hh \
	hmac.hh hmac.cc \
	wforce_exception.hh \
	sholder.hh \
	sodcrypto.cc sodcrypto.hh \
	luastate.hh \
	sstuff.hh ext/luawrapper/include/LuaContext.hpp \
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
	$(LUA_LIBS) $(YAHTTP_LIBS) ${libsodium_LIBS} $(GEOIP_LIBS) $(GETDNS_LIBS)  $(PROTOBUF_LIBS) \
	$(LIBSYSTEMD_LIBS) $(JSON11_LIBS) $(BOOST_DATE_TIME_LIBS) $(LIBHIREDIS_LIBS) $(LIBCURL) $(LIBCRYPTO_LIBS)

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
	wforce.hh \
	replication.hh \
	replication_sdb.hh \
	replication_bl.hh \
	twmap-wrapper.hh \
	perf-stats.hh \
	webhook.hh \
	minicurl.hh \
	hmac.hh \
	wforce_exception.hh \
	wforce_ns.hh \
	luastate.hh

replication.pb.cc: replication.proto
	$(AM_V_GEN)$(PROTOC) --cpp_out=./ $<

nodist_wforce_SOURCES = replication.pb.cc replication.pb.h

wforce.$(OBJEXT): replication.pb.cc

if HAVE_SYSTEMD
SYSTEMD_TYPE = notify
systemdsystemunitdir = $(SYSTEMD_DIR)
systemdsystemunit_DATA = wforce.service
else
SYSTEMD_TYPE = simple
endif
wforce.service: wforce.service.in
	$(AM_V_GEN)sed -e 's![@]bindir[@]!$(bindir)!' -e 's![@]type[@]!$(SYSTEMD_TYPE)!' < $< > $@
