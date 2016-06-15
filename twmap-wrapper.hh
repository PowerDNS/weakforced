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
  bool setFields(const std::vector<pair<std::string, std::string>>& fmvec);
  bool setFields(const FieldMap& fm);
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
  void resetInternal(const TWKeyType vkey, bool replicate=true);
  unsigned int get_size();
  void set_size_soft(unsigned int size);
};

extern std::mutex dbMap_mutx;
extern std::map<std::string, TWStringStatsDBWrapper> dbMap;
