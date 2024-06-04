
# Contributors

LuaLDAP was originally created by the [Kepler Project](http://www.keplerproject.org/)
 (designed and implemented by Roberto Ierusalimschy, Andr&eacute; Carregal and Tom&aacute;s Guisasola).

A CVS-to-Git conversion was done by [LuaForge](http://luaforge.net/)
from their [project](http://luaforge.net/projects/lualdap/)
to a [GitHub repository](https://github.com/luaforge/lualdap).

From there, the community took over and moved in a new repository
under the [LuaLDAP GitHub organisation](https://github.com/lualdap).
The following maintainers curated fixes, improvements and releases:

 * [Brett Delle Grazie](https://github.com/bdellegrazie) : 2014 - 2015
 * [Dennis Schridde](https://github.com/devurandom) : 2017 - 2019
 * [Fran&ccedil;ois Perrad](https://github.com/fperrad) : 2021 - 2023

Significant contributions were made by (in order of appearance):

 * [Matthew Wild](https://github.com/mwild1) <mwild1@gmail.com>: Bugfixes; curation of community patches
 * [Dennis Schridde](https://github.com/devurandom) <devurandom@gmx.net>: Lua 5.1, 5.2, 5.3 compatibility; OpenLDAP 2.3 compatibility; initial CircleCI and Codecov setup; bugfixes; curation of community patches
 * Michael Bienia <geser@ubuntu.com>, Micah Gersten <micahg@ubuntu.com>, Luca Capello <luca@pca.it>: [Bugfix for Debian](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=722175)
 * [John Regan](https://github.com/jprjr) <john@jrjrtech.com>: Initial Rockspec
 * [Dan Callaghan](https://github.com/danc86) <djc@djc.id.au>: Initial test harness using a live OpenLDAP server; bugfixes
 * [Brett Delle Grazie](https://github.com/bdellegrazie) <brett.dellegrazie@indigoblue.co.uk>: Documentation updates; curation of community patches
 * [Jakub Jirutka](https://github.com/jirutka) <jakub@jirutka.cz>: Initial Travis CI setup; bugfixes
 * [Victor Seva](https://github.com/linuxmaniac) <linuxmaniac@torreviejawireless.org>: `lualdap` global variable only with Lua 5.1
 * [Fran&ccedil;ois Perrad](https://github.com/fperrad) <francois.perrad@gadz.org>: Lua 5.4 compatiblity, fix WinLDAP
 * [Daurnimator](https://github.com/daurnimator) - Cleanup
 * [Cyril Romain](https://github.com/cyrilRomain) <cyril@romain.tf>: Fix segfault in `search()` with multiple attrs
 * [Will Robertson](https://github.com/wspr) <wspr81@gmail.com>: Improve example with `search()` in manual
 * [Alex Dowad](https://github.com/alexdowad) <alexinbeijing@gmail.com>: Add optional timeout argument to open and open_simple
