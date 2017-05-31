AM_CPPFLAGS = $(LUA_CFLAGS) $(YAHTTP_CFLAGS) $(WFORCE_CFLAGS) $(EXT_CFLAGS) $(libsodium_CFLAGS) $(GETDNS_CFLAGS) $(JSON11_CFLAGS) $(LIBHIREDIS_CFLAGS) $(LIBCURL_CFLAGS) $(LIBCRYPTO_INCLUDES) $(YAMLCPP_FLAGS) -O3 -Wall -pthread -DSYSCONFDIR='"$(sysconfdir)"'
ACLOCAL_AMFLAGS = -I m4

UAP_REGEX_FILE=regexes.yaml

if WITH_DOCKER
BUILD_DOCKER = docker
endif
SUBDIRS=ext docs common trackalert $(BUILD_DOCKER)

EXTRA_DIST= wforce.conf README.md wforce.service.in wforce.conf.example replication.proto $(UAP_REGEX_FILE)

CLEANFILES = replication.pb.cc replication.pb.h wforce.service $(UAP_REGEX_FILE)
distclean-local:
	-rm -rf *.dSYM

sysconf_DATA= wforce.conf wforce.conf.example $(UAP_REGEX_FILE)

bin_PROGRAMS = wforce

wforce_SOURCES = \
	wforce.cc wforce.hh \
	wforce-lua.cc \
	wforce-web.cc wforce-web.hh\
	twmap.cc \
	twmap-wrapper.cc twmap-wrapper.hh \
	blacklist.cc blacklist.hh \
	replication.cc replication.hh \
	replication_sdb.hh replication_sdb.cc \
	replication_bl.cc replication_bl.hh \
	luastate.hh

wforce_LDFLAGS = \
	$(AM_LDFLAGS) \
	-pthread 

wforce_LDADD = \
	-lreadline -ltermcap \
	$(LUA_LIBS) $(YAHTTP_LIBS) $(EXT_LIBS) $(WFORCE_LIBS) \
	${libsodium_LIBS} $(GEOIP_LIBS) $(GETDNS_LIBS)  $(PROTOBUF_LIBS) \
	$(LIBSYSTEMD_LIBS) $(JSON11_LIBS) $(BOOST_DATE_TIME_LIBS) \
	$(BOOST_REGEX_LIBS) $(LIBHIREDIS_LIBS) $(LIBCURL) $(LIBCRYPTO_LIBS) \
	$(YAMLCPP_LIBS)

noinst_HEADERS = \
	blacklist.hh \
	wforce.hh \
	replication.hh \
	replication_sdb.hh \
	replication_bl.hh \
	twmap-wrapper.hh \
	luastate.hh \
	wforce-web.hh

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
