-- LuaRocks configuration

rocks_trees = {
   { name = "user", root = home .. "/.luarocks" };
   { name = "system", root = "/Users/ncook/GitProjects/weakforced/luaenv/target" };
}
lua_interpreter = "luajit";
variables = {
   LUA_DIR = "/Users/ncook/GitProjects/weakforced/luaenv/target";
   LUA_BINDIR = "/Users/ncook/GitProjects/weakforced/luaenv/target/bin";
}
