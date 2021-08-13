AM_CPPFLAGS = $(LUA_CFLAGS) \
	$(EXT_CFLAGS) \
	$(WFORCE_CFLAGS) \
	$(libsodium_CFLAGS) \
	$(JSON11_CFLAGS) \
	$(LIBCURL_CFLAGS) \
	$(LIBCRYPTO_INCLUDES) \
	$(LIBDROGON_INCLUDES) \
    $(LIBJSONCPP_INCLUDES) \
    $(LIBZ_INCLUDES) \
	$(LIBPROMETHEUS_INCLUDES) \
	$(YAMLCPP_FLAGS) \
	$(MMDB_CFLAGS) \
	-pthread \
	-DSYSCONFDIR='"$(sysconfdir)"' \
	-I$(top_srcdir)/wforce

AM_LDFLAGS = \
	$(PROGRAM_LDFLAGS) \
	$(THREADFLAGS) \
	$(SANITIZER_FLAGS)

EXTRA_DIST= trackalert.conf README.md trackalert.service.in

CLEANFILES = trackalert.service
distclean-local:
	-rm -rf *.dSYM

sysconf_DATA= trackalert.conf 
bin_PROGRAMS = trackalert
trackalert_SOURCES = \
	trackalert.cc trackalert.hh \
	trackalert-lua.cc \
	trackalert-web.cc trackalert-web.hh\
	trackalert-luastate.hh

trackalert_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LIBPROMETHEUS_LDFLAGS) \
    $(LIBDROGON_LDFLAGS) \
    $(LIBJSONCPP_LDFLAGS) \
    $(LIBCRYPTO_LDFLAGS) \
    $(LIBZ_LDFLAGS)
	-pthread 

trackalert_LDADD = \
	-lreadline -ltermcap \
	$(LUA_LIBS) $(EXT_LIBS) $(WFORCE_LIBS) \
	$(libsodium_LIBS) \
	$(LIBSYSTEMD_LIBS) $(JSON11_LIBS) $(BOOST_DATE_TIME_LIBS) \
	$(BOOST_REGEX_LIBS) $(LIBCURL) $(LIBCRYPTO_LIBS) \
	$(LIBDROGON_LIBS) $(LIBJSONCPP_LIBS) $(LIBZ_LIBS) $(LIBSSL_LIBS) \
	$(YAMLCPP_LIBS) $(GEOIP_LIBS) $(MMDB_LIBS) $(LIBPROMETHEUS_LIBS)

EXTRA_trackalert_DEPENDENCIES = \
	$(EXT_LIBS) $(WFORCE_LIBS)

noinst_HEADERS = \
	trackalert.hh \
	trackalert-luastate.hh \
	trackalert-web.hh

if HAVE_SYSTEMD
SYSTEMD_TYPE = notify
systemdsystemunitdir = $(SYSTEMD_DIR)
systemdsystemunit_DATA = trackalert.service
else
SYSTEMD_TYPE = simple
endif
trackalert.service: trackalert.service.in
	$(AM_V_GEN)sed -e 's![@]bindir[@]!$(bindir)!' -e 's![@]type[@]!$(SYSTEMD_TYPE)!' < $< > $@
