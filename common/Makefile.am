AM_CPPFLAGS = $(LUA_CFLAGS) $(YAHTTP_CFLAGS) $(EXT_CFLAGS) $(libsodium_CFLAGS) $(GETDNS_CFLAGS) $(JSON11_CFLAGS) $(LIBCURL_CFLAGS) $(LIBCRYPTO_INCLUDES) $(YAMLCPP_FLAGS) -O3 -Wall -pthread -DSYSCONFDIR='"$(sysconfdir)"'

noinst_LTLIBRARIES=libweakforce.la
libweakforce_la_LIBADD=$(EXT_LIBS) $(YAHTTP_LIBS) $(JSON11_LIBS) $(BOOST_FILESYSTEM_LIBS) $(BOOST_SYSTEM_LIBS)
libweakforce_la_SOURCES= \
	base64.hh \
	dns_lookup.cc dns_lookup.hh \
	wforce-geoip.cc wforce-geoip.hh \
	twmap.hh \
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
	login_tuple.cc login_tuple.hh

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
	wforce-geoip.hh \
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
	login_tuple.hh
