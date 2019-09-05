/*
 * This file is part of PowerDNS or weakforced.
 * Copyright -- PowerDNS.COM B.V. and its contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
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

#include "twmap.hh"

typedef boost::variant<std::string, int, ComboAddress> TWKeyType;

// This is a Lua-friendly wrapper to the Stats DB
// All db state is stored in the TWStatsDB shared pointer because passing back/forth to Lua means copies
class TWStringStatsDBWrapper
{
private:
  std::shared_ptr<TWStatsDB<std::string>> sdbp;
  std::shared_ptr<bool> replicated;
public:

  TWStringStatsDBWrapper(const std::string& name, int window_size, int num_windows);
  TWStringStatsDBWrapper(const std::string& name, int window_size, int num_windows, const std::vector<pair<std::string, std::string>>& fmvec);

  void enableReplication();
  void disableReplication();
  bool getReplicationStatus();
  std::string getDBName();
  bool setFields(const std::vector<pair<std::string, std::string>>& fmvec);
  bool setFields(const FieldMap& fm);
  const FieldMap& getFields();
  void setv4Prefix(uint8_t bits);
  void setv6Prefix(uint8_t bits);
  std::string getStringKey(const TWKeyType vkey);
  void add(const TWKeyType vkey, const std::string& field_name, const boost::variant<std::string, int, ComboAddress>& param1, boost::optional<int> param2);
  void addInternal(const TWKeyType vkey, const std::string& field_name, const boost::variant<std::string, int, ComboAddress>& param1, boost::optional<int> param2, bool replicate=true);
  void sub(const TWKeyType vkey, const std::string& field_name, const boost::variant<std::string, int>& val);
  void subInternal(const TWKeyType vkey, const std::string& field_name, const boost::variant<std::string, int>& val, bool replicate=true);
  int get(const TWKeyType vkey, const std::string& field_name, const boost::optional<boost::variant<std::string, ComboAddress>> param1);
  int get_current(const TWKeyType vkey, const std::string& field_name, const boost::optional<boost::variant<std::string, ComboAddress>> param1);
  std::vector<int> get_windows(const TWKeyType vkey, const std::string& field_name, const boost::optional<boost::variant<std::string, ComboAddress>> param1);
  bool get_all_fields(const TWKeyType vkey, std::vector<std::pair<std::string, int>>& ret_vec);
  void reset(const TWKeyType vkey);
  void resetField(const TWKeyType vkey, const std::string& field_name);
  void resetInternal(const TWKeyType vkey, bool replicate=true);
  void resetFieldInternal(const TWKeyType vkey, const std::string& field_name, bool replicate=true);
  unsigned int get_size();
  unsigned int get_max_size();
  void set_size_soft(unsigned int size);
  int windowSize();
  int numWindows();
  const std::list<std::string>::iterator startDBDump();
  bool DBDumpEntry(std::list<std::string>::iterator& i,
                   TWStatsDBDumpEntry& entry,
                   std::string& key);
  const std::list<std::string>::iterator DBDumpIteratorEnd();
  void endDBDump();
  void restoreEntry(const std::string& key, TWStatsDBDumpEntry& entry);
  void startExpireThread();
};

extern std::mutex dbMap_mutx;
extern std::map<std::string, TWStringStatsDBWrapper> dbMap;
