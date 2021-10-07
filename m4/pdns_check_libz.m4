# SYNOPSIS
#
#   PDNS_CHECK_LIBZ([action-if-found[, action-if-not-found]])
#
# DESCRIPTION
#
#   Look for libz in a number of default spots, or in a
#   user-selected spot (via --with-libz).  Sets
#
#     LIBZ_INCLUDES to the include directives required
#     LIBZ_LIBS to the -l directives required
#     LIBZ_LDFLAGS to the -L or -R flags required
#
#   and calls ACTION-IF-FOUND or ACTION-IF-NOT-FOUND appropriately
#
#
# LICENSE
#
# Taken and modified from AX_CHECK_OPENSSL by:
#   Copyright (c) 2009,2010 Zmanda Inc. <http://www.zmanda.com/>
#   Copyright (c) 2009,2010 Dustin J. Mitchell <dustin@zmanda.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 1

AU_ALIAS([CHECK_LIBZ], [PDNS_CHECK_LIBZ])
AC_DEFUN([PDNS_CHECK_LIBZ], [
    found=false
    AC_ARG_WITH([libz],
        [AS_HELP_STRING([--with-libz=DIR],
            [root of the zlib directory])],
        [
            case "$withval" in
            "" | y | ye | yes | n | no)
            AC_MSG_ERROR([Invalid --with-libz value])
              ;;
            *) libzdirs="$withval"
              ;;
            esac
        ], [
            # if pkg-config is installed and libz has installed a .pc file,
            # then use that information and don't search libzdirs
            AC_CHECK_TOOL([PKG_CONFIG], [zlib pkg-config])
            if test x"$PKG_CONFIG" != x""; then
                LIBZ_LDFLAGS=`$PKG_CONFIG zlib --libs-only-L 2>/dev/null`
                if test $? = 0; then
                    LIBZ_LIBS=`$PKG_CONFIG zlib --libs-only-l 2>/dev/null`
                    LIBZ_INCLUDES=`$PKG_CONFIG zlib --cflags-only-I 2>/dev/null`
                    found=true
                fi
            fi

            # no such luck; use some default dirs
            if ! $found; then
                libzdirs="/usr/local /usr/local/opt /usr/pkg /usr"
            fi
        ]
        )

    if ! $found; then
        LIBZ_INCLUDES=
        for libzdir in $libzdirs; do
            AC_MSG_CHECKING([for zlib.h in $libzdir])
            if test -f "$libzdir/include/zlib.h"; then
                LIBZ_INCLUDES="-I$libzdir/include"
                LIBZ_LDFLAGS="-L$libzdir/lib"
                LIBZ_LIBS="-lz"
                found=true
                AC_MSG_RESULT([yes])
                break
            else
                AC_MSG_RESULT([no])
            fi
        done
    fi

    if ! $found; then
        AC_MSG_NOTICE([Did not find zlib])
        $2
    else
        AC_MSG_NOTICE([Found zlib])
        $1
    fi

    AC_SUBST([LIBZ_INCLUDES])
    AC_SUBST([LIBZ_LIBS])
    AC_SUBST([LIBZ_LDFLAGS])
])
