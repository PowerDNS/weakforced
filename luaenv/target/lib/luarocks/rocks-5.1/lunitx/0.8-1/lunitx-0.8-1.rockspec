package = "lunitx"
version = "0.8-1"
source = {
  url = "git://github.com/dcurrie/lunit.git",
  tag = "0.8.1"
}
description = {
  summary = "Lunitx is a unit testing framework for lua, written in lua.",
  detailed = [[
    Lunitx is a unit testing framework for lua, written in lua,
    based heavily on Lunit 0.5, but modified to work with Lua 5.2.
    Lunitx provides 27 assert functions, and a few misc functions
    for usage in an easy unit testing framework.
    Lunit comes with a test suite to test itself. The testsuite
    consists of approximately 710 assertions.
  ]],
  homepage = "https://github.com/dcurrie/lunit",
  license = "MIT/X11"
}
dependencies = {
  "lua >= 5.1, < 5.5"
}
build = {
  type = "builtin",
  modules = {
    ["lunit"] = "lua/lunit.lua",
    ["lunitx"] = "lua/lunitx.lua",
    ["lunit.console"] = "lua/lunit/console.lua",
    ["lunitx.atexit"] = "lua/lunitx/atexit.lua",
  },
  install = {
    bin = {
      ["lunit.sh"] = "extra/lunit.sh",
    }
  }
}
