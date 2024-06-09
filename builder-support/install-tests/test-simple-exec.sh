#!/bin/sh

set -ex

/usr/bin/wforce --help >/dev/null
/usr/bin/trackalert --help >/dev/null

# isempty() is an extension only present in the OpenResty fork of LuaJIT,
# the import will error in the original LuaJIT and Lua.
/usr/bin/wforce-luajit -e 'local isempty = require "table.isempty" print(isempty({}))'

# Check Lua modules are loaded
/usr/bin/wforce-luajit -e 'require "elasticsearch" require "lualdap"'

# Check that our lua-dist is the first libluajit that is found
/sbin/ldconfig --print-cache | grep '/usr/share/wforce-lua-dist/lib/exported/libluajit'
ldd /usr/bin/wforce | grep '/usr/share/wforce-lua-dist/lib/exported/libluajit'