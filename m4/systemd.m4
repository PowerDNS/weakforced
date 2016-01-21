# Copyright (c) 2011-2013, MichaÅ‚ GÃ³rny
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.


# SYSTEMD_DIRECTORY_AC(directory-variable, directory-description)
# ---------------------------------------------------------------
#
# A generic macro that obtains one of the systemd directories defined
# in systemd.pc. It adds --with-$1 configure option and tries to
# obtain the default from pkg-config.
#
# If any location was found or given, the name given as $1 will be
# substituted with it. Otherwise, $with_[$1] will be set to 'no'.

AC_DEFUN([SYSTEMD_DIRECTORY_AC], [
	AC_REQUIRE([PKG_PROG_PKG_CONFIG])

	AC_ARG_WITH([$1],
		AS_HELP_STRING([--with-$1=DIR],
			[Directory for $2 (default: auto-detect through pkg-config)]))

	AC_MSG_CHECKING([$2 directory])

	AS_IF([test x"$with_$1" = x"yes" -o x"$with_$1" = x""], [
		ac_systemd_pkgconfig_dir=`$PKG_CONFIG --variable=$1 systemd`

		AS_IF([test x"$ac_systemd_pkgconfig_dir" = x""], [
			AS_IF([test x"$with_$1" = x"yes"], [
				AC_MSG_ERROR([systemd support requested but pkg-config unable to query systemd package])
			])
			with_$1=no
		], [
			with_$1=$ac_systemd_pkgconfig_dir
		])
	])

	AC_MSG_RESULT([$with_$1])

	AS_IF([test x"$with_$1" != x"no"], [
		AC_SUBST([$1], [$with_$1])
	])
])

# SYSTEMD_DIRECTORY_AM(directory-variable)
# ---------------------------------------------------------------
#
# Defines WITH_$1 automake macro if directory was set through
# --with-$1 or obtained from pkg-config.

AC_DEFUN([SYSTEMD_DIRECTORY_AM], [
	m4_pushdef([AM_MACRO], WITH_[]m4_toupper([$1]))
	AM_CONDITIONAL(AM_MACRO,
		[test x"$with_$1" != x"no"])
	m4_popdef([AM_MACRO])
])

# SYSTEMD_SYSTEMUNITDIR_AC
# ------------------------
#
# A macro grabbing all information necessary to install systemd system
# units. It adds --with-systemdsystemunitdir (with defaults from
# pkg-config) and either gets the correct location for systemd system
# units or the request not to install them.
#
# If installing system units was requested, systemdsystemunitdir will be
# substituted with a correct location; otherwise,
# $with_systemdsystemunitdir will be set to 'no'.
#
# This macro is intended for use only in specific projects not using
# automake. Projects using automake should use the non-AC variant
# instead.

AC_DEFUN([SYSTEMD_SYSTEMUNITDIR_AC], [
	SYSTEMD_DIRECTORY_AC([systemdsystemunitdir], [systemd system unit])
])

# SYSTEMD_SYSTEMUNITDIR
# ---------------------
#
# An extended version of SYSTEMD_SYSTEMUNITDIR_AC with automake support.
#
# In addition to substituting systemdsystemunitdir, it creates
# an automake conditional called WITH_SYSTEMDSYSTEMUNITDIR.
#
# Example use:
# - configure.ac:
#	SYSTEMD_SYSTEMUNITDIR
# - Makefile.am:
#	if WITH_SYSTEMDSYSTEMUNITDIR
#	dist_systemdsystemunit_DATA = foo.service
#	endif

AC_DEFUN([SYSTEMD_SYSTEMUNITDIR], [
	AC_REQUIRE([SYSTEMD_SYSTEMUNITDIR_AC])
	SYSTEMD_DIRECTORY_AM([systemdsystemunitdir])
])

# SYSTEMD_SYSTEMPRESETDIR_AC
# --------------------------
#
# A macro grabbing all information necessary to install systemd system
# presets. It adds --with-systemdsystempresetdir (with defaults from
# pkg-config) and either gets the correct location for systemd system
# presets or the request not to install them.
#
# If installing system presets was requested, systemdsystempresetdir
# will be substituted with a correct location; otherwise,
# $with_systemdsystempresetdir will be set to 'no'.
#
# This macro is intended for use only in specific projects not using
# automake. Projects using automake should use the non-AC variant
# instead.

AC_DEFUN([SYSTEMD_SYSTEMPRESETDIR_AC], [
	SYSTEMD_DIRECTORY_AC([systemdsystempresetdir], [systemd system preset])
])

# SYSTEMD_SYSTEMPRESETDIR
# -----------------------
#
# An extended version of SYSTEMD_SYSTEMPRESETDIR_AC with automake
# support.
#
# In addition to substituting systemdsystempresetdir, it creates
# an automake conditional called WITH_SYSTEMDSYSTEMPRESETDIR.

AC_DEFUN([SYSTEMD_SYSTEMPRESETDIR], [
	AC_REQUIRE([SYSTEMD_SYSTEMPRESETDIR_AC])
	SYSTEMD_DIRECTORY_AM([systemdsystempresetdir])
])

# SYSTEMD_SYSTEMCONFDIR_AC
# ------------------------
#
# A macro grabbing all information necessary to install systemd system
# configuration. It adds --with-systemdsystemconfdir (with defaults from
# pkg-config) and either gets the correct location for systemd system
# configuration or the request not to install them.
#
# If installing to the systemconfdir was requested, systemdsystemconfdir
# will be substituted with a correct location; otherwise,
# $with_systemdsystemconfdir will be set to 'no'.
#
# This macro is intended for use only in specific projects not using
# automake. Projects using automake should use the non-AC variant
# instead.

AC_DEFUN([SYSTEMD_SYSTEMCONFDIR_AC], [
	SYSTEMD_DIRECTORY_AC([systemdsystemconfdir], [systemd system configuration])
])

# SYSTEMD_SYSTEMCONFDIR
# ---------------------
#
# An extended version of SYSTEMD_SYSTEMCONFDIR_AC with automake
# support.
#
# In addition to substituting systemdsystemconfdir, it creates
# an automake conditional called WITH_SYSTEMDSYSTEMCONFDIR.

AC_DEFUN([SYSTEMD_SYSTEMCONFDIR], [
	AC_REQUIRE([SYSTEMD_SYSTEMCONFDIR_AC])
	SYSTEMD_DIRECTORY_AM([systemdsystemconfdir])
])

# SYSTEMD_USERUNITDIR_AC
# ----------------------
#
# A macro grabbing all information necessary to install systemd user
# units. It adds --with-systemduserunitdir (with defaults from
# pkg-config) and either gets the correct location for systemd user
# units or the request not to install them.
#
# If installing user units was requested, systemduserunitdir will be
# substituted with a correct location; otherwise,
# $with_systemduserunitdir will be set to 'no'.
#
# This macro is intended for use only in specific projects not using
# automake. Projects using automake should use the non-AC variant
# instead.

AC_DEFUN([SYSTEMD_USERUNITDIR_AC], [
	SYSTEMD_DIRECTORY_AC([systemduserunitdir], [systemd user unit])
])

# SYSTEMD_USERUNITDIR
# -------------------
#
# An extended version of SYSTEMD_USERUNITDIR_AC with automake support.
#
# In addition to substituting systemduserunitdir, it creates
# an automake conditional called WITH_SYSTEMDUSERUNITDIR.
#
# Example use:
# - configure.ac:
#	SYSTEMD_USERUNITDIR
# - Makefile.am:
#	if WITH_SYSTEMDUSERUNITDIR
#	dist_systemduserunit_DATA = foo.service
#	endif

AC_DEFUN([SYSTEMD_USERUNITDIR], [
	AC_REQUIRE([SYSTEMD_USERUNITDIR_AC])
	SYSTEMD_DIRECTORY_AM([systemduserunitdir])
])

# SYSTEMD_USERPRESETDIR_AC
# ------------------------
#
# A macro grabbing all information necessary to install systemd user
# presets. It adds --with-systemduserpresetdir (with defaults from
# pkg-config) and either gets the correct location for systemd user
# presets or the request not to install them.
#
# If installing user presets was requested, systemduserpresetdir
# will be substituted with a correct location; otherwise,
# $with_systemduserpresetdir will be set to 'no'.
#
# This macro is intended for use only in specific projects not using
# automake. Projects using automake should use the non-AC variant
# instead.

AC_DEFUN([SYSTEMD_USERPRESETDIR_AC], [
	SYSTEMD_DIRECTORY_AC([systemduserpresetdir], [systemd user preset])
])

# SYSTEMD_USERPRESETDIR
# ---------------------
#
# An extended version of SYSTEMD_USERPRESETDIR_AC with automake
# support.
#
# In addition to substituting systemduserpresetdir, it creates
# an automake conditional called WITH_SYSTEMDUSERPRESETDIR.

AC_DEFUN([SYSTEMD_USERPRESETDIR], [
	AC_REQUIRE([SYSTEMD_USERPRESETDIR_AC])
	SYSTEMD_DIRECTORY_AM([systemduserpresetdir])
])

# SYSTEMD_USERCONFDIR_AC
# ----------------------
#
# A macro grabbing all information necessary to install systemd user
# configuration. It adds --with-systemduserconfdir (with defaults from
# pkg-config) and either gets the correct location for systemd user
# configuration or the request not to install them.
#
# If installing to the userconfdir was requested, systemduserconfdir
# will be substituted with a correct location; otherwise,
# $with_systemduserconfdir will be set to 'no'.
#
# This macro is intended for use only in specific projects not using
# automake. Projects using automake should use the non-AC variant
# instead.

AC_DEFUN([SYSTEMD_USERCONFDIR_AC], [
	SYSTEMD_DIRECTORY_AC([systemduserconfdir], [systemd user configuration])
])

# SYSTEMD_USERCONFDIR
# -------------------
#
# An extended version of SYSTEMD_USERCONFDIR_AC with automake
# support.
#
# In addition to substituting systemduserconfdir, it creates
# an automake conditional called WITH_SYSTEMDUSERCONFDIR.

AC_DEFUN([SYSTEMD_USERCONFDIR], [
	AC_REQUIRE([SYSTEMD_USERCONFDIR_AC])
	SYSTEMD_DIRECTORY_AM([systemduserconfdir])
])

# SYSTEMD_UTILDIR_AC
# ------------------
#
# A macro grabbing all information necessary to obtain systemd utility
# directory. It adds --with-systemdutildir (with defaults from
# pkg-config) and either gets the correct location for systemd
# utility programs or the request not to install them.
#
# If installing to the utildir was requested, systemdutildir
# will be substituted with a correct location; otherwise,
# $with_systemdutildir will be set to 'no'.
#
# This macro is intended for use only in specific projects not using
# automake. Projects using automake should use the non-AC variant
# instead.

AC_DEFUN([SYSTEMD_UTILDIR_AC], [
	SYSTEMD_DIRECTORY_AC([systemdutildir], [systemd utility])
])

# SYSTEMD_UTILDIR
# ---------------
#
# An extended version of SYSTEMD_UTILDIR_AC with automake
# support.
#
# In addition to substituting systemdutildir, it creates
# an automake conditional called WITH_SYSTEMDUTILDIR.

AC_DEFUN([SYSTEMD_UTILDIR], [
	AC_REQUIRE([SYSTEMD_UTILDIR_AC])
	SYSTEMD_DIRECTORY_AM([systemdutildir])
])

# SYSTEMD_SYSTEMGENERATORDIR_AC
# -----------------------------
#
# A macro grabbing all information necessary to install systemd system
# generators. It adds --with-systemdsystemgeneratordir (with defaults from
# pkg-config) and either gets the correct location for systemd system
# generators or the request not to install them.
#
# If installing system generators was requested, systemdsystemgeneratordir
# will be substituted with a correct location; otherwise,
# $with_systemdsystemgeneratordir will be set to 'no'.
#
# This macro is intended for use only in specific projects not using
# automake. Projects using automake should use the non-AC variant
# instead.

AC_DEFUN([SYSTEMD_SYSTEMGENERATORDIR_AC], [
	SYSTEMD_DIRECTORY_AC([systemdsystemgeneratordir], [systemd system generator])
])

# SYSTEMD_SYSTEMGENERATORDIR
# --------------------------
#
# An extended version of SYSTEMD_SYSTEMGENERATORDIR_AC with automake
# support.
#
# In addition to substituting systemdsystemgeneratordir, it creates
# an automake conditional called WITH_SYSTEMDSYSTEMGENERATORDIR.

AC_DEFUN([SYSTEMD_SYSTEMGENERATORDIR], [
	AC_REQUIRE([SYSTEMD_SYSTEMGENERATORDIR_AC])
	SYSTEMD_DIRECTORY_AM([systemdsystemgeneratordir])
])

# SYSTEMD_USERGENERATORDIR_AC
# ---------------------------
#
# A macro grabbing all information necessary to install systemd user
# generators. It adds --with-systemdusergeneratordir (with defaults from
# pkg-config) and either gets the correct location for systemd user
# generators or the request not to install them.
#
# If installing user generators was requested, systemdusergeneratordir
# will be substituted with a correct location; otherwise,
# $with_systemdusergeneratordir will be set to 'no'.
#
# This macro is intended for use only in specific projects not using
# automake. Projects using automake should use the non-AC variant
# instead.

AC_DEFUN([SYSTEMD_USERGENERATORDIR_AC], [
	SYSTEMD_DIRECTORY_AC([systemdusergeneratordir], [systemd user generator])
])

# SYSTEMD_USERGENERATORDIR
# ------------------------
#
# An extended version of SYSTEMD_USERGENERATORDIR_AC with automake
# support.
#
# In addition to substituting systemdusergeneratordir, it creates
# an automake conditional called WITH_SYSTEMDUSERGENERATORDIR.

AC_DEFUN([SYSTEMD_USERGENERATORDIR], [
	AC_REQUIRE([SYSTEMD_USERGENERATORDIR_AC])
	SYSTEMD_DIRECTORY_AM([systemdusergeneratordir])
])

# SYSTEMD_CATALOGDIR_AC
# ---------------------
#
# A macro grabbing all information necessary to install Journal
# catalogs. It adds --with-catalogdir (with defaults from pkg-config)
# and either gets the correct location for catalogs or the request
# not to install them.
#
# If installing catalogs was requested, catalogdir will be substituted
# with a correct location; otherwise, $with_catalogdir will be set
# to 'no'.
#
# This macro is intended for use only in specific projects not using
# automake. Projects using automake should use the non-AC variant
# instead.

AC_DEFUN([SYSTEMD_CATALOGDIR_AC], [
	SYSTEMD_DIRECTORY_AC([catalogdir], [Journal catalog])
])

# SYSTEMD_CATALOGDIR
# ------------------
#
# An extended version of SYSTEMD_CATALOGDIR_AC with automake
# support.
#
# In addition to substituting catalogdir, it creates an automake
# conditional called WITH_CATALOGDIR.

AC_DEFUN([SYSTEMD_CATALOGDIR], [
	AC_REQUIRE([SYSTEMD_CATALOGDIR_AC])
	SYSTEMD_DIRECTORY_AM([catalogdir])
])

# SYSTEMD_MISC
# ------------
#
# Declare miscellaneous (unconditional) directories used by systemd,
# and possibly other init systems.
#
# Declared directories:
# - binfmtdir (binfmt.d for binfmt_misc decl files),
# - modulesloaddir (modules-load.d for module loader).
# - sysctldir (sysctl.d for /proc/sys settings),
# - tmpfilesdir (tmpfiles.d for temporary file setup).
#
# Example use:
# - configure.ac:
#	SYSTEMD_MISC
# - Makefile.am:
#	dist_binfmt_DATA = binfmt/foo.conf
#
# TODO: systemd only supports /usr and /usr/local

AC_DEFUN([SYSTEMD_MISC], [
	AS_IF([test x"$prefix" = x"/"], [
		AC_SUBST([binfmtdir], [/usr/lib/binfmt.d])
		AC_SUBST([modulesloaddir], [/usr/lib/modules-load.d])
		AC_SUBST([sysctldir], [/usr/lib/sysctl.d])
		AC_SUBST([tmpfilesdir], [/usr/lib/tmpfiles.d])
		AC_SUBST([kernelinstalldir], [/usr/lib/kernel/install.d])
		AC_SUBST([ntpunitsdir], [/usr/lib/systemd/ntp-units.d])
	], [
		AC_SUBST([binfmtdir], ['${prefix}/lib/binfmt.d'])
		AC_SUBST([modulesloaddir], ['${prefix}/lib/modules-load.d'])
		AC_SUBST([sysctldir], ['${prefix}/lib/sysctl.d'])
		AC_SUBST([tmpfilesdir], ['${prefix}/lib/tmpfiles.d'])
		AC_SUBST([kernelinstalldir], ['${prefix}/lib/kernel/install.d'])
		AC_SUBST([ntpunitsdir], ['${prefix}/lib/systemd/ntp-units.d'])
	])
])


# Obsolete macros.
AU_ALIAS([SYSTEMD_SYSTEM_UNITS_AC], [SYSTEMD_SYSTEMUNITDIR_AC])
AU_DEFUN([SYSTEMD_SYSTEM_UNITS], [
	SYSTEMD_SYSTEMUNITDIR

	AM_CONDITIONAL([WITH_SYSTEMD_SYSTEM_UNITS],
		[test x"$with_systemdsystemunitdir" != x"no"])
], [Please replace WITH_SYSTEMD_SYSTEM_UNITS automake conditionals with WITH_SYSTEMDSYSTEMUNITDIR and drop the definition of the former.])
