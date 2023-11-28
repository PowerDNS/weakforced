# SYNOPSIS
#
#   PDNS_CHECK_LIBPROMETHEUS([action-if-found[, action-if-not-found]])
#
# DESCRIPTION
#
#   Look for prometheus-cpp library in standard places, or in a
#   user-selected spot (via --with-libprometheus).  Sets
#
#     LIBPROMETHEUS_INCLUDES to the include directives required
#     LIBPROMETHEUS_LIBS to the -l directives required
#     LIBPROMETHEUS_LDFLAGS to the -L or -R flags required
#
#   and calls ACTION-IF-FOUND or ACTION-IF-NOT-FOUND appropriately
#
#   This macro sets LIBPROMETHEUS_INCLUDES such that source files should use the
#   prometheus/ directory in include directives:
#
#     #include <prometheus/registry.h>
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

AU_ALIAS([CHECK_LIBPROMETHEUS], [PDNS_CHECK_LIBPROMETHEUS])
AC_DEFUN([PDNS_CHECK_LIBPROMETHEUS], [
    found=false
    AC_ARG_WITH([libprometheus],
        [AS_HELP_STRING([--with-libprometheus=DIR],
            [root of the OpenSSL directory])],
        [
            case "$withval" in
            "" | y | ye | yes | n | no)
            AC_MSG_ERROR([Invalid --with-libprometheus value])
              ;;
            *) promdirs="$withval"
              ;;
            esac
        ], [
            # if pkg-config is installed and there is a .pc file,
            # then use that information and don't search promdirs
            AC_CHECK_TOOL([PKG_CONFIG], [pkg-config])
            if test x"$PKG_CONFIG" != x""; then
                LIBPROMETHEUS_LDFLAGS=`$PKG_CONFIG prometheus-cpp-core --libs-only-L 2>/dev/null`
                if test $? = 0; then
                    LIBPROMETHEUS_LIBS=`$PKG_CONFIG prometheus-cpp-core --libs-only-l 2>/dev/null`
                    LIBPROMETHEUS_INCLUDES=`$PKG_CONFIG prometheus-cpp-core --cflags-only-I 2>/dev/null`
                    found=true
                fi
            fi

            # no such luck; use some default promdirs
            if ! $found; then
                promdirs="/usr/local /usr"
            fi
        ]
        )


    # note that we #include <prometheus/foo.h>, so the prometheus headers have to be in
    # an 'prometheus' subdirectory

    if ! $found; then
        LIBPROMETHEUS_INCLUDES=
        for promdir in $promdirs; do
            AC_MSG_CHECKING([for prometheus/registry.h in $promdir])
            if test -f "$promdir/include/prometheus/registry.h"; then
                LIBPROMETHEUS_INCLUDES="-I$promdir/include"
                LIBPROMETHEUS_LDFLAGS="-L$promdir/lib"
                LIBPROMETHEUS_LIBS="-lprometheus-cpp-core"
                found=true
                AC_MSG_RESULT([yes])
                break
            else
                AC_MSG_RESULT([no])
            fi
        done

        # if the file wasn't found, well, go ahead and try the link anyway -- maybe
        # it will just work!
    fi

    # try the preprocessor and linker with our new flags,
    # being careful not to pollute the global LIBS, LDFLAGS, and CPPFLAGS

    AC_MSG_CHECKING([whether compiling and linking against libprometheus-cpp-core works])
    echo "Trying link with LIBPROMETHEUS_LDFLAGS=$LIBPROMETHEUS_LDFLAGS;" \
        "LIBPROMETHEUS_LIBS=$LIBPROMETHEUS_LIBS; LIBPROMETHEUS_INCLUDES=$LIBPROMETHEUS_INCLUDES" >&AS_MESSAGE_LOG_FD

    save_LIBS="$LIBS"
    save_LDFLAGS="$LDFLAGS"
    save_CPPFLAGS="$CPPFLAGS"
    LDFLAGS="$LDFLAGS $LIBPROMETHEUS_LDFLAGS"
    LIBS="$LIBPROMETHEUS_LIBS $LIBS"
    CPPFLAGS="$LIBPROMETHEUS_INCLUDES $CPPFLAGS"
    AC_LINK_IFELSE(
        [AC_LANG_PROGRAM([#include <prometheus/registry.h>], [prometheus::Registry reg;])],
        [
            AC_MSG_RESULT([yes])
            $1
        ], [
            AC_MSG_RESULT([no])
            $2
        ])
    CPPFLAGS="$save_CPPFLAGS"
    LDFLAGS="$save_LDFLAGS"
    LIBS="$save_LIBS"

    AC_SUBST([LIBPROMETHEUS_INCLUDES])
    AC_SUBST([LIBPROMETHEUS_LIBS])
    AC_SUBST([LIBPROMETHEUS_LDFLAGS])
])
