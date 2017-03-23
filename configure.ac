AC_INIT([wforce], [1.0.3-rc1])
AM_INIT_AUTOMAKE([foreign dist-bzip2 parallel-tests 1.11 subdir-objects])
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_PROG_CXX
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([GEOIP], [geoip], [AC_DEFINE([HAVE_GEOIP], [1], [Define to 1 if you have geoip])], [true])
PKG_CHECK_MODULES([GETDNS], [getdns], [AC_DEFINE([HAVE_GETDNS], [1], [Define to 1 if you have getdns])], [true])
PKG_CHECK_MODULES([libsodium], [libsodium], [AC_DEFINE([HAVE_LIBSODIUM], [1], [Define to 1 if you have libsodium])])
PKG_CHECK_MODULES([LIBSYSTEMD], [libsystemd], [AC_DEFINE([HAVE_LIBSYSTEMD], [1], [Define to 1 if you have libsystemd-dev])], [true])
AM_CONDITIONAL([LIBSYSTEMD],[test "$HAVE_LIBSYSTEMD" = "1"])
AC_PROG_LIBTOOL
BOOST_REQUIRE([1.35])
BOOST_DATE_TIME
BOOST_FOREACH
AC_CHECK_PROG(PERL, perl, perl)
AX_PROG_PERL_MODULES( Swagger2::Markdown, , AC_MSG_WARN(Need to install Perl Swagger2::Markdown))
AC_SUBST([YAHTTP_CFLAGS], ['-I$(top_srcdir)/ext/yahttp'])
AC_SUBST([YAHTTP_LIBS], ['-L$(top_builddir)/ext/yahttp/yahttp -lyahttp'])
AC_SUBST([JSON11_CFLAGS], ['-I$(top_srcdir)/ext/json11'])
AC_SUBST([JSON11_LIBS], ['-L$(top_builddir)/ext/json11 -ljson11'])
PDNS_WITH_LUA
PDNS_CHECK_PANDOC
AX_CXX_COMPILE_STDCXX_11(ext,mandatory)
AM_CONDITIONAL([CXX2011],[test "$HAVE_CXX11" = "1"])
AX_AVAILABLE_SYSTEMD
AM_CONDITIONAL([HAVE_SYSTEMD], [ test x"$systemd" = "xy" ])
AC_CONFIG_FILES([Makefile
	ext/Makefile
	ext/json11/Makefile
	ext/yahttp/Makefile
	ext/yahttp/yahttp/Makefile
	docs/Makefile])
	
AC_OUTPUT
