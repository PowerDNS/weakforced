#pragma once

#include "twmap.hh"

typedef boost::variant<std::string, int, ComboAddress> TWKeyType;

// This is a Lua-friendly wrapper to the Stats DB
// All db state is stored in the TWStatsDB shared pointer because passing back/forth to Lua means copies
class TWStringStatsDBWrapper
{
private:
  std::shared_ptr<TWStatsDB<std::string>> sdbp;
  bool replicated=false;
public:

  TWStringStatsDBWrapper(const std::string& name, int window_size, int num_windows)
  {
    sdbp = std::make_shared<TWStatsDB<std::string>>(name, window_size, num_windows);
    thread t(TWStatsDB<std::string>::twExpireThread, sdbp);
    t.detach();
  }

  TWStringStatsDBWrapper(const std::string& name, int window_size, int num_windows, const std::vector<pair<std::string, std::string>>& fmvec)
  {
    sdbp = std::make_shared<TWStatsDB<std::string>>(name, window_size, num_windows);    
    (void)setFields(fmvec);
    thread t(TWStatsDB<std::string>::twExpireThread, sdbp);
    t.detach();
  }

  void enableReplication()
  {
    replicated = true;
  }

  void disableReplication()
  {
    replicated = false;
  }

  bool setFields(const std::vector<pair<std::string, std::string>>& fmvec) {
    FieldMap fm;
    for(const auto& f : fmvec) {
      fm.insert(std::make_pair(f.first, f.second));
    }
    return sdbp->setFields(fm);
  }

  bool setFields(const FieldMap& fm) 
  {
    return(sdbp->setFields(fm));
  }

  void setv4Prefix(uint8_t bits)
  {
    uint8_t v4_prefix = bits > 32 ? 32 : bits;
    sdbp->setv4Prefix(v4_prefix);
  }

  void setv6Prefix(uint8_t bits)
  {
    uint8_t v6_prefix = bits > 128 ? 128 : bits;
    sdbp->setv6Prefix(v6_prefix);
  }

  std::string getStringKey(const TWKeyType vkey)
  {
    if (vkey.which() == 0) {
      return boost::get<std::string>(vkey);
    }
    else if (vkey.which() == 1) {
      return std::to_string(boost::get<int>(vkey));
    }
    else if (vkey.which() == 2) {
      const ComboAddress ca =  boost::get<ComboAddress>(vkey);
      if (ca.isIpv4()) {
	uint8_t v4_prefix = sdbp->getv4Prefix();
	return Netmask(ca, v4_prefix).toStringNetwork();
      }
      else if (ca.isIpv6()) {
	uint8_t v6_prefix = sdbp->getv6Prefix();
	return Netmask(ca, v6_prefix).toStringNetwork();
      }
    }
    return std::string{};
  }

  void add(const TWKeyType vkey, const std::string& field_name, const boost::variant<std::string, int, ComboAddress>& param1, boost::optional<int> param2) 
  {
    std::string key = getStringKey(vkey);

    // we're using the three argument version
    if (param2) {
      if ((param1.which() != 0) && (param1.which() != 2))
	return;
      else if (param1.which() == 0) {
	std::string mystr = boost::get<std::string>(param1);
	sdbp->add(key, field_name, mystr, *param2);
      }
      else if (param1.which() == 2) {
	ComboAddress ca = boost::get<ComboAddress>(param1);
	sdbp->add(key, field_name, ca.toString(), *param2);
      }
    }
    else {
      if (param1.which() == 0) {
	std::string mystr = boost::get<std::string>(param1);
	sdbp->add(key, field_name, mystr);
      }
      else if (param1.which() == 1) {
	int myint = boost::get<int>(param1);
	sdbp->add(key, field_name, myint);
      }
      else if (param1.which() == 2) {
	ComboAddress ca = boost::get<ComboAddress>(param1);
	sdbp->add(key, field_name, ca.toString());
      }
    }
  }

  void sub(const TWKeyType vkey, const std::string& field_name, const boost::variant<std::string, int>& val)
  {
    std::string key = getStringKey(vkey);
    if (val.which() == 0) {
      std::string mystr = boost::get<std::string>(val);
      sdbp->sub(key, field_name, mystr);
    }
    else {
      int myint = boost::get<int>(val);
      sdbp->sub(key, field_name, myint);
    }
  }
  
  int get(const TWKeyType vkey, const std::string& field_name, const boost::optional<boost::variant<std::string, ComboAddress>> param1)
  {
    std::string key = getStringKey(vkey);
    if (param1) {
      if (param1->which() == 0) {
	std::string s = boost::get<std::string>(*param1);  
	return sdbp->get(key, field_name, s);
      }
      else {
	ComboAddress ca = boost::get<ComboAddress>(*param1);
	return sdbp->get(key, field_name, ca.toString());
      }
    }
    else {
      return sdbp->get(key, field_name);
    }
  }

  int get_current(const TWKeyType vkey, const std::string& field_name, const boost::optional<boost::variant<std::string, ComboAddress>> param1)
  {
    std::string key = getStringKey(vkey);
    if (param1) {
      if (param1->which() == 0) {
	std::string s = boost::get<std::string>(*param1);  
	return sdbp->get_current(key, field_name, s);
      }
      else {
	ComboAddress ca = boost::get<ComboAddress>(*param1);
	return sdbp->get_current(key, field_name, ca.toString());
      }
    }
    else {
      return sdbp->get_current(key, field_name);
    }
  }

  std::vector<int> get_windows(const TWKeyType vkey, const std::string& field_name, const boost::optional<boost::variant<std::string, ComboAddress>> param1)
  {
    std::vector<int> retvec;
    
    std::string key = getStringKey(vkey);
    if (param1) {
      if (param1->which() == 0) {
	std::string s = boost::get<std::string>(*param1);  
	(void) sdbp->get_windows(key, field_name, s, retvec);
      }
      else {
	ComboAddress ca = boost::get<ComboAddress>(*param1);
	(void) sdbp->get_windows(key, field_name, ca.toString(), retvec);
      }      
    }
    else
      (void) sdbp->get_windows(key, field_name, retvec);

    return retvec; // copy
  }

  bool get_all_fields(const TWKeyType vkey, std::vector<std::pair<std::string, int>>& ret_vec)
  {
    std::string key = getStringKey(vkey);
    return sdbp->get_all_fields(key, ret_vec);
  }

  void reset(const TWKeyType vkey)
  {
    std::string key = getStringKey(vkey);
    sdbp->reset(key);
  }

  unsigned int get_size()
  {	
    return sdbp->get_size();
  }

  void set_size_soft(unsigned int size) 
  {
    sdbp->set_map_size_soft(size);
  }

};

extern std::mutex dbMap_mutx;
extern std::map<std::string, TWStringStatsDBWrapper> dbMap;
