# SYNOPSIS
#
#   PDNS_CHECK_YAMLCPP([action-if-found[, action-if-not-found]])
#
# DESCRIPTION
#
#   Look for yaml-cpp in a number of default spots, or in a
#   user-selected spot (via --with-yamlcpp).  Sets
#
#     YAMLCPP_INCLUDES to the include directives required
#     YAMLCPP_LIBS to the -l directives required
#     YAMLCPP_LDFLAGS to the -L or -R flags required
#
#   and calls ACTION-IF-FOUND or ACTION-IF-NOT-FOUND appropriately
#
#   This macro sets YAMLCPP_INCLUDES such that source files should use the
#   yaml-cpp/ directory in include directives:
#
#     #include <yaml-cpp/yaml.h>
#
# LICENSE
#
# Taken and modified from PDNS_CHECK_OPENSSL by:
#   Copyright (c) 2009,2010 Zmanda Inc. <http://www.zmanda.com/>
#   Copyright (c) 2009,2010 Dustin J. Mitchell <dustin@zmanda.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 1

AU_ALIAS([CHECK_YAMLCPP], [PDNS_CHECK_YAMLCPP])
AC_DEFUN([PDNS_CHECK_YAMLCPP], [
    found=false
    AC_ARG_WITH([yamlcpp],
        [AS_HELP_STRING([--with-yamlcpp=DIR],
            [root of the yaml-cpp directory])],
        [
            case "$withval" in
            "" | y | ye | yes | n | no)
            AC_MSG_ERROR([Invalid --with-yamlcpp value])
              ;;
            *) yamlcppdirs="$withval"
              ;;
            esac
        ], [
            # if pkg-config is installed and yaml-cpp has installed a .pc file,
            # then use that information and don't search yamlcppdirs
            AC_CHECK_TOOL([PKG_CONFIG], [pkg-config])
            if test x"$PKG_CONFIG" != x""; then
                YAMLCPP_LDFLAGS=`$PKG_CONFIG yaml-cpp --libs-only-L 2>/dev/null`
                if test $? = 0; then
                    YAMLCPP_LIBS=`$PKG_CONFIG yaml-cpp --libs-only-l 2>/dev/null`
                    YAMLCPP_INCLUDES=`$PKG_CONFIG yaml-cpp --cflags-only-I 2>/dev/null`
                    found=true
                fi
            fi

            # no such luck; use some default yamlcppdirs
            if ! $found; then
                yamlcppdirs="/usr /usr/local /usr/lib /usr/pkg /usr"
            fi
        ]
        )


    # note that we #include <yaml-cpp/foo.h>, so the YAML-CPP headers have to be in
    # an 'yaml-cpp' subdirectory

    if ! $found; then
        YAMLCPP_INCLUDES=
        for yamlcppdir in $yamlcppdirs; do
            AC_MSG_CHECKING([for yaml-cpp/yaml.h in $ssldir])
            if test -f "$ssldir/include/yaml-cpp/yaml.h"; then
                YAMLCPP_INCLUDES="-I$ssldir/include"
                YAMLCPP_LDFLAGS="-L$ssldir/lib"
                YAMLCPP_LIBS="-lyaml-cpp"
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

    AC_MSG_CHECKING([whether compiling and linking against yaml-cpp works])
    echo "Trying link with YAMLCPP_LDFLAGS=$YAMLCPP_LDFLAGS;" \
        "YAMLCPP_LIBS=$YAMLCPP_LIBS; YAMLCPP_INCLUDES=$YAMLCPP_INCLUDES" >&AS_MESSAGE_LOG_FD

    save_LIBS="$LIBS"
    save_LDFLAGS="$LDFLAGS"
    save_CPPFLAGS="$CPPFLAGS"
    LDFLAGS="$LDFLAGS $YAMLCPP_LDFLAGS"
    LIBS="$YAMLCPP_LIBS $LIBS"
    CPPFLAGS="$YAMLCPP_INCLUDES $CPPFLAGS"
    AC_LINK_IFELSE(
        [AC_LANG_PROGRAM([#include <yaml-cpp/yaml.h>], [YAML::Node node;])],
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

    AC_SUBST([YAMLCPP_INCLUDES])
    AC_SUBST([YAMLCPP_LIBS])
    AC_SUBST([YAMLCPP_LDFLAGS])
])
