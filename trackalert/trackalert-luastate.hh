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
#include "customfunc.hh"

struct CustomFuncMapObject {
  custom_func_t c_func;
};

typedef std::map<std::string, CustomFuncMapObject> CustomFuncMap;
extern CustomFuncMap g_custom_func_map;

typedef std::function<void(const LoginTuple&)> report_t;
extern report_t g_report;
typedef std::function<void()> background_t;
extern background_t g_background;
typedef std::unordered_map<std::string, background_t> bg_func_map_t;

vector<std::function<void(void)>> setupLua(bool client, bool allow_report, LuaContext& c_lua, report_t& report_func, bg_func_map_t* bg_func_map, CustomFuncMap& custom_func_map, const std::string& config);

struct LuaThreadContext {
  LuaContext lua_context;
  std::mutex lua_mutex;
  report_t report_func;
  bg_func_map_t bg_func_map;
  CustomFuncMap custom_func_map;
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
      lua_cv.push_back(std::make_shared<LuaThreadContext>());
    }	
  }

  LuaMultiThread(const LuaMultiThread&) = delete;
  LuaMultiThread& operator=(const LuaMultiThread&) = delete;
  
  // these are used to setup the function pointers
  std::vector<std::shared_ptr<LuaThreadContext>>::iterator begin() { return lua_cv.begin(); }
  std::vector<std::shared_ptr<LuaThreadContext>>::iterator end() { return lua_cv.end(); }

  void report(const LoginTuple& lt) {
    auto lt_context = getLuaState();
    // lock the lua state mutex
    std::lock_guard<std::mutex> lock(lt_context->lua_mutex);
    // call the report function
    lt_context->report_func(lt);
  }

  void background(const std::string& func_name) {
    auto lt_context = getLuaState();
    // lock the lua state mutex
    std::lock_guard<std::mutex> lock(lt_context->lua_mutex);
    // call the background function
    auto fn = lt_context->bg_func_map.find(func_name);
    if (fn != lt_context->bg_func_map.end())
      fn->second();
  }

  CustomFuncReturn custom_func(const std::string& command, const CustomFuncArgs& cfa) {
    auto lt_context = getLuaState();
    // lock the lua state mutex
    std::lock_guard<std::mutex> lock(lt_context->lua_mutex);
    // call the custom function
    for (const auto& i : lt_context->custom_func_map) {
      if (command.compare(i.first) == 0) {
	return i.second.c_func(cfa);
      }
    }
    return CustomFuncReturn(false, KeyValVector{});
  }
  
protected:
  std::shared_ptr<LuaThreadContext> getLuaState()
  {
    std::lock_guard<std::mutex> lock(mutx);
    if (state_index >= num_states)
      state_index = 0;
    return lua_cv[state_index++];
  }
private:
  std::vector<std::shared_ptr<LuaThreadContext>> lua_cv;
  unsigned int num_states;
  unsigned int state_index;
  std::mutex mutx;
};

extern std::shared_ptr<LuaMultiThread> g_luamultip;
extern int g_num_luastates;

