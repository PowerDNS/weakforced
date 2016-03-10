lib_LTLIBRARIES=libyahttp.la
include_yahttpdir=$(includedir)/yahttp
include_yahttp_HEADERS=cookie.hpp exception.hpp reqresp.hpp router.hpp url.hpp utility.hpp yahttp.hpp yahttp-config.h
libyahttp_la_CXXFLAGS=-W -Wall $(RELRO_CFLAGS) $(PIE_CFLAGS) -D__STRICT_ANSI__
libyahttp_la_SOURCES=cookie.hpp exception.hpp reqresp.cpp reqresp.hpp router.cpp router.hpp url.hpp utility.hpp yahttp.hpp
