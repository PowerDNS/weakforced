package = 'luasystem'
version = '0.2.1-0'
source = {
  url = "https://github.com/o-lim/luasystem/archive/v0.2.1.tar.gz",
  dir = "luasystem-0.2.1"
}
description = {
  summary = 'Platform independent system calls for Lua.',
  detailed = [[
    Adds a Lua API for making platform independent system calls.
  ]],
  homepage = 'http://olivinelabs.com/luasystem/',
  license = 'MIT <http://opensource.org/licenses/MIT>'
}
dependencies = {
  'lua >= 5.1',
}

local function make_platform(plat)
  local defines = {
    linux = { },
    unix = { },
    macosx = { },
    win32 = { "WINVER=0x0600", "_WIN32_WINNT=0x0600" },
    mingw32 = { "WINVER=0x0600", "_WIN32_WINNT=0x0600" },
  }
  local libraries = {
    linux = { "rt" },
    unix = { },
    macosx = { },
    win32 = { },
    mingw32 = { },
  }
  return {
    modules = {
      ['system.core'] = {
        sources = { 'src/core.c', 'src/compat.c', 'src/time.c', },
        defines = defines[plat],
        libraries = libraries[plat],
      },
    },
  }
end

build = {
  type = 'builtin',
  platforms = {
    linux = make_platform('linux'),
    unix = make_platform('unix'),
    macosx = make_platform('macosx'),
    win32 = make_platform('win32'),
    mingw32 = make_platform('mingw32'),
  },
  modules = {
    ['system.init'] = 'system/init.lua',
  },
}
