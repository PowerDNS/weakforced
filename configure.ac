AC_INIT([wforce], [2.12.1])
AM_INIT_AUTOMAKE([foreign dist-bzip2 parallel-tests 1.11 subdir-objects tar-ustar])
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_HOST
: ${CFLAGS="-Wall -g -O3"}
: ${CXXFLAGS="-Wall -g -O3"}

AC_PROG_CC
AC_PROG_CXX
AC_LANG([C++])
PDNS_CHECK_OS
LT_INIT

# Do we want to do a docker-only build?
AC_ARG_ENABLE([docker-only],
[  --enable-docker-only         Build and run docker images only, don't check for compilation dependencies],
[case "${enableval}" in
  yes) docker_only=true ;;
  no)  docker_only=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-docker]) ;;
esac],[docker_only=false])

# If docker-only then don't check for all the dependencies
AS_IF([test "x$docker_only" = "xfalse"], [

# Check for C++ 2017 support
AX_CXX_COMPILE_STDCXX_17(ext,mandatory)

# Check dependencies with pkgconfig
PKG_PROG_PKG_CONFIG
AC_ARG_ENABLE([geoip],
 AS_HELP_STRING([--disable-geoip], [ Disable geoip feature ]))
AS_IF([test "x$enable_geoip" != "xno"], [
 AC_DEFINE([WITH_GEOIP], [1], [Geoip is enabled])
 PDNS_CHECK_GEOIP
 AS_IF([test "x$MMDB_LIBS" = "x"], [
  AC_MSG_ERROR([libmaxmindb not found, libmaxmind is required"])
 ])
])
AC_CHECK_LIB(dl,dlopen,DLLIBS="-ldl",,)
AC_SUBST(DLLIBS, $DLLIBS)
# Drogon uses std::filesystem but older versions of g++ need an extra library to use it
AC_MSG_NOTICE([Checking for stdc++fs])
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -lstdc++fs"
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([#include <filesystem>],
    [std::filesystem::path mypath])],
  [STDCXXFSLIBS="-lstdc++fs"] [HAVE_LIBSTDCXXFS=1],
  [AC_MSG_NOTICE([libstdc++fs is not installed.])])
LDFLAGS=$SAVED_LDFLAGS
AC_SUBST(STDCXXFSLIBS, $STDCXXFSLIBS)
PDNS_WITH_LIBSSL
AC_ARG_ENABLE([dns],
AS_HELP_STRING([--disable-dns], [ Disable getdns lookup feature ]))
AS_IF([test "x$enable_dns" != "xno"], [
 AC_DEFINE([WITH_GETDNS], [1], [Getdns is enabled])
 PKG_CHECK_MODULES([GETDNS], [getdns >= 1.2.0], [AC_DEFINE([HAVE_GETDNS], [1], [Define to 1 if you have getdns])])
])
AC_ARG_ENABLE([sodium],
 AS_HELP_STRING([--disable-sodium], [ Disable Libsodium support ]))
AS_IF([test "x$enable_sodium" != "xno"], [
 PKG_CHECK_MODULES([libsodium], [libsodium >= 1.0.10], [AC_DEFINE([HAVE_LIBSODIUM], [1], [Define to 1 if you have libsodium])])
])
PKG_CHECK_MODULES([LIBSYSTEMD], [libsystemd], [AC_DEFINE([HAVE_LIBSYSTEMD], [1], [Define to 1 if you have libsystemd-dev])], [true])
PKG_CHECK_MODULES([LIBHIREDIS], [libhiredis], [AC_DEFINE([HAVE_LIBHIREDIS], [1], [Define to 1 if you have libhiredis-dev])], [true])
AS_IF([test "x$LIBHIREDIS_LIBS" = "x"], [
  PKG_CHECK_MODULES([LIBHIREDIS], [hiredis], [AC_DEFINE([HAVE_LIBHIREDIS], [1], [Define to 1 if you have hiredis])])
])
AS_IF([test "x$LIBHIREDIS_LIBS" = "x"], [
  AC_MSG_ERROR([libhiredis not found, libhiredis support is required])
])
PDNS_CHECK_YAMLCPP([AC_DEFINE([HAVE_YAMLCPP], [1], [Define to 1 if you have yaml-cpp])], [true])
AS_IF([test "x$YAMLCPP_LIBS" = "x"], [
  AC_MSG_ERROR([yaml-cpp not found, yaml-cpp support is required])
])
AM_CONDITIONAL([LIBSYSTEMD],[test "$HAVE_LIBSYSTEMD" = "1"])

# We need readline
WFORCE_CHECK_READLINE
# Boost-specific checks
BOOST_REQUIRE([1.42])
BOOST_DATE_TIME
AS_IF([test -z "$BOOST_DATE_TIME_LIBS"], [AC_MSG_ERROR([Boost date_time library is not installed])])
BOOST_REGEX
AS_IF([test -z "$BOOST_REGEX_LIBS"], [ AC_MSG_ERROR([Boost regex library is not installed])])
BOOST_FOREACH
BOOST_SYSTEM
BOOST_FILESYSTEM
AS_IF([test -z "$BOOST_FILESYSTEM_LIBS"], [ AC_MSG_ERROR([Boost filesystem library is not installed])])
AM_CONDITIONAL([HAVE_BOOST_GE_148], [test "$boost_major_version" -ge 148])
PDNS_ENABLE_UNIT_TESTS
PDNS_ENABLE_SANITIZERS
# Check for dependent programs
AC_CHECK_PROG(PERL, perl, perl)
AC_CHECK_PROG(HAVE_WGET, wget, wget)
AS_IF([test "x$HAVE_WGET" = "x"], [
  AC_MSG_ERROR([wget not found, wget needed for downloading regexes.yaml and is required])
])
AX_PROG_PERL_MODULES( Swagger2::Markdown, , AC_MSG_WARN(Need to install Perl Swagger2::Markdown))
# Look for protobuf
PDNS_WITH_PROTOBUF
AS_IF([test "x$PROTOBUF_LIBS" = "x" -o "x$PROTOC" = "x"], [
  AC_MSG_ERROR([Protobuf not found, protobuf support is required])
])
# Setup variables for locally built libraries
AC_SUBST([YAHTTP_CFLAGS], ['-I$(top_srcdir)/ext/yahttp'])
AC_SUBST([YAHTTP_LIBS], ['-L$(abs_top_builddir)/ext/yahttp/yahttp -lyahttp'])
AC_SUBST([JSON11_CFLAGS], ['-I$(top_srcdir)/ext/json11'])
AC_SUBST([JSON11_LIBS], ['-L$(abs_top_builddir)/ext/json11 -ljson11'])
AC_SUBST([EXT_CFLAGS], ['-I$(top_srcdir)/ext'])
AC_SUBST([EXT_LIBS], ['$(abs_top_builddir)/ext/ext/libext.la'])
AC_SUBST([WFORCE_CFLAGS], ['-I$(top_srcdir)'/common])
AC_SUBST([WFORCE_LIBS], ['$(abs_top_builddir)/common/libweakforce.la'])
# We need libcrypto for hash functions
PDNS_CHECK_LIBCRYPTO([], [
  AC_MSG_ERROR([Libcrypto not found, libcrypto support is required])
  ]
)
# We need libdrogon for HTTP/HTTPS functions
PDNS_CHECK_LIBDROGON([], [
  AC_MSG_ERROR([Libdrogon not found, libdrogon support is required])
  ]
)
# Drogon depends on libjsoncpp, libuuid and libz
PDNS_CHECK_LIBJSONCPP([], [
  AC_MSG_ERROR([Libjsoncpp not found, libjsoncpp is required as a mandatory dependency of libdrogon])
  ]
)
PDNS_CHECK_LIBUUID([], [
  AC_MSG_ERROR([Libuuid not found, libuuid is required as a mandatory dependency of libdrogon])
  ]
)
PDNS_CHECK_LIBZ([], [
  AC_MSG_ERROR([Libz not found, libz support is required as a mandatory dependency of libdrogon])
  ]
)
# Check for LuaJIT first then Lua
PDNS_WITH_LUAJIT
AS_IF([test "x$with_luajit" = "xno"], [
  PDNS_WITH_LUA
])
AS_IF([test "x$LUAPC" = "x" -a "x$LUAJITPC" = "x"], [
  AC_MSG_ERROR([Neither Lua nor LuaJIT found, Lua support is required])
])
PDNS_CHECK_LUA_HPP
# Need asciidoctor to build documentation
AC_CHECK_PROG(HAVE_ASCIIDOCTOR, asciidoctor, asciidoctor)
AS_IF([test "x$HAVE_ASCIIDOCTOR" = "x"], [
  AC_MSG_ERROR([asciidoctor not found, needed to generate manpages and is required])
])

# We need prometheus-cpp
PDNS_CHECK_LIBPROMETHEUS([], [
  AC_MSG_ERROR([Libprometheus not found, libprometheus-cpp support is required])
  ]
)
# Check for systemd
AX_AVAILABLE_SYSTEMD
AM_CONDITIONAL([HAVE_SYSTEMD], [ test x"$systemd" = "xy" ])
# We need a working libcurl package
LIBCURL_CHECK_CONFIG
AS_IF([test "x$LIBCURL" = "x"], [
  AC_MSG_ERROR([Libcurl not found, libcurl support is required])
])

PTHREAD_SET_NAME

# Docker-only
])

AS_IF([ test "x$docker_only" = "xtrue" ], [
AM_CONDITIONAL([HAVE_SYSTEMD], [ test x = x ])
AM_CONDITIONAL([LIBSYSTEMD], [ test x = x ])
AM_CONDITIONAL([HAVE_LIBSSL], [ test x = x ])
AM_CONDITIONAL([HAVE_BOOST_GE_148], [ test x = x ])
AM_CONDITIONAL([UNIT_TESTS], [ test x = x ])
AM_CONDITIONAL([BACKEND_UNIT_TESTS], [ test x = x ])
AM_CONDITIONAL([HAVE_PROTOBUF], [ test x = x ])
AM_CONDITIONAL([HAVE_PROTOC], [ test x = x ])
AM_CONDITIONAL([LUA], [ test x = x ])
AM_CONDITIONAL([HAVE_LUA_HPP], [ test x = x ])
AM_CONDITIONAL([BACKEND_UNIT_TESTS], [ test x = x ])
AM_CONDITIONAL([BACKEND_UNIT_TESTS], [ test x = x ])
AM_CONDITIONAL([BACKEND_UNIT_TESTS], [ test x = x ])
AM_CONDITIONAL([BACKEND_UNIT_TESTS], [ test x = x ])
])

# Do we want the docker subdirectories to be built?
AC_ARG_ENABLE([docker],
[  --enable-docker         Build and run docker images],
[case "${enableval}" in
  yes) docker=true ;;
  no)  docker=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-docker]) ;;
esac],[docker=false])
AM_CONDITIONAL([WITH_DOCKER], [test x$docker = xtrue])
AC_CHECK_PROG(DOCKER, docker, docker)
AS_IF([test x$docker = xtrue -a "x$DOCKER" = "x"], [
    AC_MSG_ERROR([docker not found, and is required])
])
AC_ARG_ENABLE([trackalert],
[  --enable-trackalert         Build trackalert],
[case "${enableval}" in
  yes) trackalert=true ;;
  no)  trackalert=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-trackalert]) ;;
esac],[trackalert=false])
AM_CONDITIONAL([WITH_TRACKALERT], [test x$trackalert = xtrue])
AC_CONFIG_FILES([Makefile
	ext/Makefile
	ext/ext/Makefile
	ext/json11/Makefile
	common/Makefile
	docs/Makefile
	docker/Makefile
        docker/wforce_image/Makefile
	trackalert/Makefile
        wforce/Makefile])
AC_OUTPUT
