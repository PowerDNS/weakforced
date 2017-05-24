AM_CPPFLAGS = $(LUA_CFLAGS) $(YAHTTP_CFLAGS) $(libsodium_CFLAGS) $(GETDNS_CFLAGS) $(JSON11_CFLAGS) $(LIBHIREDIS_CFLAGS) $(LIBCURL_CFLAGS) $(LIBCRYPTO_INCLUDES) $(YAMLCPP_FLAGS) -O3 -Wall -pthread -DSYSCONFDIR='"$(sysconfdir)"'
ACLOCAL_AMFLAGS = -I m4

UAP_REGEX_FILE=regexes.yaml

if WITH_DOCKER
BUILD_DOCKER = docker
endif
SUBDIRS=ext docs trackalert $(BUILD_DOCKER)

EXTRA_DIST= wforce.conf README.md wforce.service.in wforce.conf.example replication.proto $(UAP_REGEX_FILE)

CLEANFILES = replication.pb.cc replication.pb.h wforce.service $(UAP_REGEX_FILE)
distclean-local:
	-rm -rf *.dSYM

sysconf_DATA= wforce.conf wforce.conf.example $(UAP_REGEX_FILE)
bin_PROGRAMS = wforce
wforce_SOURCES = \
	base64.hh \
	dns_lookup.cc dns_lookup.hh \
	wforce.cc wforce.hh \
	wforce-lua.cc \
	wforce-web.cc wforce-web.hh\
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
	lock.hh \
	pdnsexception.hh \
	device_parser.hh device_parser.cc \
	wforce-webserver.cc wforce-webserver.hh \
	sstuff.hh ext/luawrapper/include/LuaContext.hpp \
	login_tuple.cc login_tuple.hh \
	ext/incbin/incbin.h \
	ext/murmur3.cc \
        ext/murmur3.h \
	ext/count_min_sketch.cpp \
	ext/ctpl.h \
	ext/hyperloglog.hpp \
	ext/count_min_sketch.hpp \
	ext/uap-cpp/UaParser.h \
	ext/uap-cpp/UaParser.cpp

wforce_LDFLAGS = \
	$(AM_LDFLAGS) \
	-pthread 

wforce_LDADD = \
	-lreadline -ltermcap \
	$(LUA_LIBS) $(YAHTTP_LIBS) ${libsodium_LIBS} $(GEOIP_LIBS) $(GETDNS_LIBS)  $(PROTOBUF_LIBS) \
	$(LIBSYSTEMD_LIBS) $(JSON11_LIBS) $(BOOST_DATE_TIME_LIBS) $(BOOST_REGEX_LIBS) $(LIBHIREDIS_LIBS) $(LIBCURL) $(LIBCRYPTO_LIBS) $(YAMLCPP_LIBS)

noinst_HEADERS = \
	base64.hh \
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
	luastate.hh \
	device_parser.hh \
	lock.hh \
	pdnsexception.hh \
	wforce-webserver.hh \
	wforce-web.hh \
	login_tuple.hh

BUILT_SOURCES = replication.pb.cc replication.pb.h
replication.pb.cc: replication.proto
	$(AM_V_GEN)$(PROTOC) -I $(top_srcdir) --cpp_out=./ $<

nodist_wforce_SOURCES = replication.pb.cc replication.pb.h

wforce.$(OBJEXT): replication.pb.cc

$(UAP_REGEX_FILE):
	wget -q https://raw.githubusercontent.com/ua-parser/uap-core/master/$(UAP_REGEX_FILE) -O $(UAP_REGEX_FILE)

if HAVE_SYSTEMD
SYSTEMD_TYPE = notify
systemdsystemunitdir = $(SYSTEMD_DIR)
systemdsystemunit_DATA = wforce.service
else
SYSTEMD_TYPE = simple
endif
wforce.service: wforce.service.in
	$(AM_V_GEN)sed -e 's![@]bindir[@]!$(bindir)!' -e 's![@]type[@]!$(SYSTEMD_TYPE)!' < $< > $@
