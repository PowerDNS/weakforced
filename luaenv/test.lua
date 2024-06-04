-- Test if we are actually running the OpenResty fork of LuaJIT
-- isempty() is an OpenResty extension: https://github.com/openresty/luajit2#tableisempty
local isempty = require "table.isempty"
print(isempty({}))

-- vendor packages, tests search path
require "socket"
require "elasticsearch"

print("OK")
