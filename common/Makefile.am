AM_CPPFLAGS = \
	$(LUA_CFLAGS) \
	$(EXT_CFLAGS) \
	$(libsodium_CFLAGS) \
	$(GETDNS_CFLAGS) \
	$(JSON11_CFLAGS) \
	$(LIBCURL_CFLAGS) \
	$(LIBCRYPTO_INCLUDES) \
	$(LIBDROGON_INCLUDES) \
	$(LIBJSONCPP_INCLUDES) \
    $(LIBZ_INCLUDES) \
	$(LIBPROMETHEUS_INCLUDES) \
	$(YAMLCPP_FLAGS) \
	$(GEOIP_CFLAGS) \
	$(MMDB_CFLAGS) \
	-pthread \
	-DSYSCONFDIR='"$(sysconfdir)"' \
	$(SANITIZER_FLAGS)

AM_LDFLAGS = \
	$(PROGRAM_LDFLAGS) \
	$(SANITIZER_FLAGS)

noinst_LTLIBRARIES=libweakforce.la
libweakforce_la_LIBADD=$(EXT_LIBS) $(JSON11_LIBS) $(BOOST_FILESYSTEM_LIBS) $(BOOST_SYSTEM_LIBS) \
                        $(GEOIP_LIBS) $(MMDB_LIBS) $(LIBPROMETHEUS_LIBS) $(LIBDROGON_LIBS) \
                        $(LIBJSONCPP_LIBS) $(LIBZ_LIBS) $(LIBCURL) $(LIBSSL_LIBS) $(LIBCRYPTO_LIBS)
libweakforce_la_SOURCES= \
	base64.hh \
	dns_lookup.cc dns_lookup.hh \
	wforce-geoip.cc wforce-geoip.hh \
	wforce-geoip2.cc wforce-geoip2.hh \
	twmap.hh twmap-static.cc customfunc.hh\
	dolog.hh wforce_ns.hh \
	iputils.cc iputils.hh \
	misc.cc misc.hh \
	namespaces.hh \
	perf-stats.cc perf-stats.hh \
	webhook.cc webhook.hh \
	minicurl.cc minicurl.hh \
	hmac.hh hmac.cc \
	wforce_exception.hh \
	sholder.hh \
	sodcrypto.cc sodcrypto.hh \
	lock.hh \
	pdnsexception.hh \
	device_parser.hh device_parser.cc \
	wforce-webserver.cc wforce-webserver.hh \
	sstuff.hh \
	login_tuple.cc login_tuple.hh \
	common-lua.cc common-lua.hh \
	prometheus.hh prometheus.cc

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
	customfunc.hh \
	wforce-geoip.hh \
	wforce-geoip2.hh \
	perf-stats.hh \
	webhook.hh \
	minicurl.hh \
	hmac.hh \
	wforce_exception.hh \
	wforce_ns.hh \
	device_parser.hh \
	lock.hh \
	pdnsexception.hh \
	wforce-webserver.hh \
	login_tuple.hh \
	common-lua.hh \
	prometheus.hh

if UNIT_TESTS

if HAVE_BOOST_GE_148
TESTS_ENVIRONMENT = env BOOST_TEST_LOG_LEVEL=message SRCDIR='$(srcdir)'
TESTS=testrunner
noinst_PROGRAMS = \
	testrunner
else
check-local:
	@echo "Unit tests disabled, boost is too old"
endif

else
check-local:
	@echo "Unit tests are not enabled"
	@echo "Run ./configure --enable-unit-tests"
endif

testrunner_SOURCES = \
	testrunner.cc \
	test-minicurl.cc \
	test-serialize.cc \
	test-crypto.cc

EXTRA_testrunner_DEPENDENCIES = \
	$(EXT_LIBS) libweakforce.la

testrunner_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS)

testrunner_LDADD = \
	$(WFORCE_LIBS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS) \
	$(LUA_LIBS) $(EXT_LIBS) $(WFORCE_LIBS) \
	${libsodium_LIBS} $(GEOIP_LIBS) $(GETDNS_LIBS)  $(PROTOBUF_LIBS) \
	$(LIBSYSTEMD_LIBS) $(JSON11_LIBS) $(BOOST_DATE_TIME_LIBS) \
	$(BOOST_REGEX_LIBS) $(LIBHIREDIS_LIBS) $(LIBCURL) $(LIBCRYPTO_LDFLAGS) \
        $(LIBCRYPTO_LIBS) $(LIBPROMETHEUS_LDFLAGS) $(LIBPROMETHEUS_LIBS)\
	$(YAMLCPP_LIBS) $(MMDB_LIBS)\
	$(BOOST_UNIT_TEST_FRAMEWORK_LIBS) \
	$(LIBDL)
