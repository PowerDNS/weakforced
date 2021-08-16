# SYNOPSIS
#
#   PDNS_CHECK_LIBJSONCPP([action-if-found[, action-if-not-found]])
#
# DESCRIPTION
#
#   Look for libjsoncpp in a number of default spots, or in a
#   user-selected spot (via --with-libjsoncpp).  Sets
#
#     LIBJSONCPP_INCLUDES to the include directives required
#     LIBJSONCPP_LIBS to the -l directives required
#     LIBJSONCPP_LDFLAGS to the -L or -R flags required
#
#   and calls ACTION-IF-FOUND or ACTION-IF-NOT-FOUND appropriately
#
#   This macro sets LIBJSONCPP_INCLUDES such that source files should use the
#   json/ directory in include directives:
#
#     #include <json/json.h>
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

AU_ALIAS([CHECK_LIBJSONCPP], [PDNS_CHECK_LIBJSONCPP])
AC_DEFUN([PDNS_CHECK_LIBJSONCPP], [
    found=false
    AC_ARG_WITH([libjsoncpp],
        [AS_HELP_STRING([--with-libjsoncpp=DIR],
            [root of the jsoncpp directory])],
        [
            case "$withval" in
            "" | y | ye | yes | n | no)
            AC_MSG_ERROR([Invalid --with-libjsoncpp value])
              ;;
            *) jsoncppdirs="$withval"
              ;;
            esac
        ], [
            # if pkg-config is installed and jsoncpp has installed a .pc file,
            # then use that information and don't search jsoncppdirs
            AC_CHECK_TOOL([PKG_CONFIG], [jsoncpp pkg-config])
            if test x"$PKG_CONFIG" != x""; then
                LIBJSONCPP_LDFLAGS=`$PKG_CONFIG jsoncpp --libs-only-L 2>/dev/null`
                if test $? = 0; then
                    LIBJSONCPP_LIBS=`$PKG_CONFIG jsoncpp --libs-only-l 2>/dev/null`
                    LIBJSONCPP_INCLUDES=`$PKG_CONFIG jsoncpp --cflags-only-I 2>/dev/null`
                    found=true
                fi
            fi

            # no such luck; use some default dirs
            if ! $found; then
                jsoncppdirs="/usr/local /usr/local/opt /usr/pkg /usr"
            fi
        ]
        )

    # note that we #include <json/json.h>, so the jsoncpp headers have to be in
    # a 'json' subdirectory

    if ! $found; then
        LIBJSONCPP_INCLUDES=
        for jsoncppdir in $jsoncppdirs; do
            AC_MSG_CHECKING([for json/json.h in $jsoncppdir])
            if test -f "$jsoncppdir/include/json/json.h"; then
                LIBJSONCPP_INCLUDES="-I$jsoncppdir/include"
                LIBJSONCPP_LDFLAGS="-L$jsoncppdir/lib"
                LIBJSONCPP_LIBS="-ljsoncpp"
                found=true
                AC_MSG_RESULT([yes])
                break
            else
                AC_MSG_RESULT([no])
            fi
        done
    fi

    if ! $found; then
        AC_MSG_NOTICE([Did not find libjsoncpp])
        $2
    else
        AC_MSG_NOTICE([Found libjsoncpp])
        $1
    fi

    AC_SUBST([LIBJSONCPP_INCLUDES])
    AC_SUBST([LIBJSONCPP_LIBS])
    AC_SUBST([LIBJSONCPP_LDFLAGS])
])
