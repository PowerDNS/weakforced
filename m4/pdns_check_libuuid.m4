# SYNOPSIS
#
#   PDNS_CHECK_LIBUUID([action-if-found[, action-if-not-found]])
#
# DESCRIPTION
#
#   Look for libuuid in a number of default spots, or in a
#   user-selected spot (via --with-libuuid).  Sets
#
#     LIBUUID_INCLUDES to the include directives required
#     LIBUUID_LIBS to the -l directives required
#     LIBUUID_LDFLAGS to the -L or -R flags required
#
#   and calls ACTION-IF-FOUND or ACTION-IF-NOT-FOUND appropriately
#
#   This macro sets LIBUUID_INCLUDES such that source files should use the
#   uuid/ directory in include directives:
#
#     #include <uuid/uuid.h>
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

AU_ALIAS([CHECK_LIBUUID], [PDNS_CHECK_LIBUUID])
AC_DEFUN([PDNS_CHECK_LIBUUID], [
    found=false
    AC_ARG_WITH([libuuid],
        [AS_HELP_STRING([--with-libuuid=DIR],
            [root of the uuid directory])],
        [
            case "$withval" in
            "" | y | ye | yes | n | no)
            AC_MSG_ERROR([Invalid --with-libuuid value])
              ;;
            *) uuiddirs="$withval"
              ;;
            esac
        ], [
            # if pkg-config is installed and uuid has installed a .pc file,
            # then use that information and don't search uuiddirs
            AC_CHECK_TOOL([PKG_CONFIG], [uuid pkg-config])
            if test x"$PKG_CONFIG" != x""; then
                LIBUUID_LDFLAGS=`$PKG_CONFIG uuid --libs-only-L 2>/dev/null`
                if test $? = 0; then
                    LIBUUID_LIBS=`$PKG_CONFIG uuid --libs-only-l 2>/dev/null`
                    LIBUUID_INCLUDES=`$PKG_CONFIG uuid --cflags-only-I 2>/dev/null`
                    found=true
                fi
            fi

            # no such luck; use some default dirs
            if ! $found; then
                uuiddirs="/usr/local /usr/local/opt /usr/pkg /usr"
            fi
        ]
        )

    # note that we #include <uuid/uuid.h>, so the uuid headers have to be in
    # a 'uuid' subdirectory

    if ! $found; then
        LIBUUID_INCLUDES=
        for uuiddir in $uuiddirs; do
            AC_MSG_CHECKING([for uuid/uuid.h in $uuiddir])
            if test -f "$uuiddir/include/uuid/uuid.h"; then
                LIBUUID_INCLUDES="-I$uuiddir/include"
                LIBUUID_LDFLAGS="-L$uuiddir/lib"
                LIBUUID_LIBS="-luuid"
                found=true
                AC_MSG_RESULT([yes])
                break
            else
                AC_MSG_RESULT([no])
            fi
        done
    fi

    if ! $found; then
        AC_MSG_NOTICE([Did not find libuuid])
        $2
    else
        AC_MSG_NOTICE([Found libuuid])
        $1
    fi

    AC_SUBST([LIBUUID_INCLUDES])
    AC_SUBST([LIBUUID_LIBS])
    AC_SUBST([LIBUUID_LDFLAGS])
])
