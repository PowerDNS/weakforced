/*
 * This file is part of PowerDNS or weakforced.
 * Copyright -- PowerDNS.COM B.V. and its contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * In addition, for the avoidance of any doubt, permission is granted to
 * link this program with OpenSSL and to (re)distribute the binaries
 * produced as the result of such linking.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once
#include "ext/luawrapper/include/LuaContext.hpp"
#include "misc.hh"
#include <mutex>
#include <thread>
#include "json11.hpp"
#include "login_tuple.hh"

typedef std::function<void(const LoginTuple&)> report_t;
extern report_t g_report;
typedef std::function<void()> background_t;
extern background_t g_background;

vector<std::function<void(void)>> setupLua(bool client, bool allow_report, LuaContext& c_lua, report_t& report_func, background_t& background_func, const std::string& config);

struct LuaThreadContext {
  std::shared_ptr<LuaContext> lua_contextp;
  std::shared_ptr<std::mutex> lua_mutexp;
  report_t report_func;
  background_t background_func;
};

#define NUM_LUA_STATES 6

class LuaMultiThread
{
public:
  LuaMultiThread() : num_states(NUM_LUA_STATES),
		     state_index(0)
  {
    LuaMultiThread(num_states);
  }

  LuaMultiThread(unsigned int nstates) : num_states(nstates),
					 state_index(0)
  {
    for (unsigned int i=0; i<num_states; i++) {
      LuaThreadContext ltc;
      ltc.lua_contextp = std::make_shared<LuaContext>();
      ltc.lua_mutexp = std::make_shared<std::mutex>();
      lua_cv.push_back(ltc);
    }	
  }

  // these are used to setup the function pointers
  std::vector<LuaThreadContext>::iterator begin() { return lua_cv.begin(); }
  std::vector<LuaThreadContext>::iterator end() { return lua_cv.end(); }

  void report(const LoginTuple& lt) {
    auto lt_context = getLuaState();
    // lock the lua state mutex
    std::lock_guard<std::mutex> lock(*(lt_context.lua_mutexp));
    // call the report function
    lt_context.report_func(lt);
  }

  void background() {
    auto lt_context = getLuaState();
    // lock the lua state mutex
    std::lock_guard<std::mutex> lock(*(lt_context.lua_mutexp));
    // call the background function
    lt_context.background_func();
  }
  
protected:
  LuaThreadContext& getLuaState()
  {
    std::lock_guard<std::mutex> lock(mutx);
    if (state_index >= num_states)
      state_index = 0;
    return lua_cv[state_index++];
  }
private:
  std::vector<LuaThreadContext> lua_cv;
  unsigned int num_states;
  unsigned int state_index;
  std::mutex mutx;
};

extern std::shared_ptr<LuaMultiThread> g_luamultip;
extern int g_num_luastates;

