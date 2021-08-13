# SYNOPSIS
#
#   PDNS_CHECK_LIBDROGON([action-if-found[, action-if-not-found]])
#
# DESCRIPTION
#
#   Look for libdrogon in a number of default spots, or in a
#   user-selected spot (via --with-libdrogon).  Sets
#
#     LIBDROGON_INCLUDES to the include directives required
#     LIBDROGON_LIBS to the -l directives required
#     LIBDROGON_LDFLAGS to the -L or -R flags required
#
#   and calls ACTION-IF-FOUND or ACTION-IF-NOT-FOUND appropriately
#
#   This macro sets LIBDROGON_INCLUDES such that source files should use the
#   drogon/ directory in include directives:
#
#     #include <drogon/drogon.h>
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

AU_ALIAS([CHECK_LIBDROGON], [PDNS_CHECK_LIBDROGON])
AC_DEFUN([PDNS_CHECK_LIBDROGON], [
    found=false
    AC_ARG_WITH([libdrogon],
        [AS_HELP_STRING([--with-libdrogon=DIR],
            [root of the drogon directory])],
        [
            case "$withval" in
            "" | y | ye | yes | n | no)
            AC_MSG_ERROR([Invalid --with-libdrogon value])
              ;;
            *) drogondirs="$withval"
              ;;
            esac
        ], [
            # if pkg-config is installed and drogon has installed a .pc file,
            # then use that information and don't search drogondirs
            AC_CHECK_TOOL([PKG_CONFIG], [libdrogon pkg-config])
            if test x"$PKG_CONFIG" != x""; then
                LIBDROGON_LDFLAGS=`$PKG_CONFIG libdrogon --libs-only-L 2>/dev/null`
                if test $? = 0; then
                    LIBDROGON_LIBS=`$PKG_CONFIG libdrogon --libs-only-l 2>/dev/null`
                    LIBDROGON_INCLUDES=`$PKG_CONFIG libdrogon --cflags-only-I 2>/dev/null`
                    found=true
                fi
            fi

            # no such luck; use some default dirs
            if ! $found; then
                drogondirs="/usr/local /usr/local/opt /usr/pkg /usr"
            fi
        ]
        )

    # note that we #include <drogon/drogon.h>, so the drogon headers have to be in
    # a 'drogon' subdirectory

    if ! $found; then
        LIBDROGON_INCLUDES=
        for drogondir in $drogondirs; do
            AC_MSG_CHECKING([for drogon/drogon.h in $drogondir])
            if test -f "$drogondir/include/drogon/drogon.h"; then
                LIBDROGON_INCLUDES="-I$drogondir/include"
                LIBDROGON_LDFLAGS="-L$drogondir/lib"
                LIBDROGON_LIBS="-ldrogon -ltrantor -ljsoncpp -lz"
                found=true
                AC_MSG_RESULT([yes])
                break
            else
                AC_MSG_RESULT([no])
            fi
        done
    fi

    if ! $found; then
        AC_MSG_NOTICE([Did not find libdrogon])
        $2
    else
        AC_MSG_NOTICE([Found libdrogon])
        $1
    fi

    AC_SUBST([LIBDROGON_INCLUDES])
    AC_SUBST([LIBDROGON_LIBS])
    AC_SUBST([LIBDROGON_LDFLAGS])
])
