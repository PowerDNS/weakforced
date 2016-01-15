#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <ctime>
#include <mutex>
#include <thread>
#include <boost/variant.hpp>
#include <boost/optional.hpp>
#include <boost/serialization/export.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <iostream>
#include <sstream>
#include "ext/hyperloglog.hpp"
#include "ext/count_min_sketch.hpp"
#include "iputils.hh"

using std::thread;

class TWStatsMember;

typedef std::shared_ptr<TWStatsMember> TWStatsMemberP;
typedef std::vector<std::pair<std::time_t, TWStatsMemberP>> TWStatsBuf;

// Not all stats member subclasses will implement all of these methods in a useful way (e.g. add string is meaningless to an integer-based class)
class TWStatsMember
{
public:
  virtual void add(int a) = 0; // add an integer to the stored value
  virtual void add(const std::string& s) = 0; // add a string to the stored window (this has different semantics for each subclass)
  virtual void add(const std::string& s, int a) = 0; // add a string/integer combo to the stored window (only applies to some stats types)
  virtual void sub(int a) = 0; // subtract an integer from the stored value
  virtual void sub(const std::string& s) = 0; // remove a string from the stored value
  virtual int get() = 0; // return the current stat
  virtual int get(const std::string& s) = 0; // return the current stat
  virtual void set(int) = 0; // Set the stored value to the supplied integer
  virtual void set(const std::string& s) = 0; // Set the stored value based on the supplied string
  virtual void erase() = 0; // Remove all data - notice this isn't necessarily the same as set(0) or set("")
  virtual int sum(const TWStatsBuf& vec) = 0; // combine an array of stored values
  virtual int sum(const std::string& s, const TWStatsBuf& vec) = 0; // combine an array of stored values
};

class TWStatsMemberInt : public TWStatsMember
{
public:
  void add(int a) { i += a; }
  void add(const std::string& s) { int a = std::stoi(s); i += a; return; }
  void add(const std::string& s, int a) { return; }
  void sub(int a) { i -= a;  }
  void sub(const std::string& s) { int a = std::stoi(s); i -= a; return; }
  int get() { return i; }
  int get(const std::string& s) { return i; }
  void set(int a) { i = a; }
  void set(const std::string& s) { return; }
  void erase() { i = 0; }
  int sum(const TWStatsBuf& vec)
  {
    int count = 0;
    for (auto a : vec)
      {
        count += a.second->get();
      }
    return count;
  }
  int sum(const std::string& s, const TWStatsBuf& vec) { return 0; }
private:
  int i=0;
};

// BOOST_CLASS_EXPORT_GUID(TWStatsMemberInt, "TWSM_INT")

#define HLL_NUM_REGISTER_BITS 10

class TWStatsMemberHLL : public TWStatsMember
{
public:
  TWStatsMemberHLL()
  {
    hllp = std::make_shared<hll::HyperLogLog>(HLL_NUM_REGISTER_BITS);
  }
  void add(int a) { char buf[64]; int len = snprintf(buf, 63, "%d", a); hllp->add(buf, len); return; }
  void add(const std::string& s) { hllp->add(s.c_str(), s.length()); return; }
  void add(const std::string& s, int a) { return; }
  void sub(int a) { return; }
  void sub(const std::string& s) { return; }
  int get() { return std::lround(hllp->estimate()); }
  int get(const std::string& s) { hllp->add(s.c_str(), s.length()); return std::lround(hllp->estimate()); } // add and return value
  void set(int a) { return; }
  void set(const std::string& s) { hllp->clear(); hllp->add(s.c_str(), s.length()); }
  void erase() { hllp->clear(); }
  int sum(const TWStatsBuf& vec)
  {
    hll::HyperLogLog hllsum(HLL_NUM_REGISTER_BITS);
    for (auto a : vec)
      {
	// XXX yes not massively pretty
	std::shared_ptr<TWStatsMemberHLL> twp = std::dynamic_pointer_cast<TWStatsMemberHLL>(a.second);
	hllsum.merge((*(twp->hllp)));
      }
    return std::lround(hllsum.estimate());
  }
  int sum(const std::string& s, const TWStatsBuf& vec) { return 0; }
private:
  std::shared_ptr<hll::HyperLogLog> hllp;
};

// BOOST_CLASS_EXPORT_GUID(TWStatsMemberHLL, "TWSM_HLL")

#define COUNTMIN_EPS 0.01
#define COUNTMIN_GAMMA 0.1

class TWStatsMemberCountMin : public TWStatsMember
{
public:
  TWStatsMemberCountMin()
  {
    cm = std::make_shared<CountMinSketch>(COUNTMIN_EPS, COUNTMIN_GAMMA);
  }
  void add(int a) { return; }
  void add(const std::string& s) { cm->update(s.c_str(), 1); }
  void add(const std::string& s, int a) { cm->update(s.c_str(), a); }
  void sub(int a) { return; }
  void sub(const std::string& s) { return; }
  int get() { return cm->totalcount(); }
  int get(const std::string& s) { return cm->estimate(s.c_str()); }
  void set(int a) { return; }
  void set(const std::string& s) { return; }
  void erase() { cm->erase(); return; }
  int sum(const TWStatsBuf& vec)
  {
    int count = 0;
    for (auto a : vec)
      {
	count += a.second->get();
      }
    return count;
  }
  int sum(const std::string&s, const TWStatsBuf& vec)
  {
    int count = 0;
    for (auto a : vec)
      {
	count += a.second->get(s);
      }
    return count;
  }
private:
  std::shared_ptr<CountMinSketch> cm;
};

// BOOST_CLASS_EXPORT_GUID(TWStatsMemberCountMin, "TWSM_CM")

template<typename T> TWStatsMemberP createInstance() { return std::make_shared<T>(); }

typedef std::map<std::string, TWStatsMemberP(*)()> map_type;

// The idea here is that the type mechanism is extensible, e.g. could add a bloom filter etc.
// This is instantiated in twmap.cc
// XXX Should make this dynamically extensible
struct TWStatsTypeMap
{
  map_type type_map;
  TWStatsTypeMap()
  {
    type_map["int"] = &createInstance<TWStatsMemberInt>;
    type_map["hll"] = &createInstance<TWStatsMemberHLL>;
    type_map["countmin"] = &createInstance<TWStatsMemberCountMin>;
  }
};

extern TWStatsTypeMap g_field_types;

class TWStatsEntry
{
public:
  TWStatsEntry(int nw, int ws, std::time_t st, const std::string field_type) {
    num_windows = nw;
    start_time = st;
    window_size = ws;
    last_cleaned = 0;
    auto it = g_field_types.type_map.find(field_type);
    if (it != g_field_types.type_map.end()) {
      for (int i=0; i< num_windows; i++) {
	stats_array.push_back(std::pair<std::time_t, TWStatsMemberP>((std::time_t)0, it->second()));
      }
    }
  }
  void add(int a) {
    std::lock_guard<std::mutex> lock(mutx);
    clean_windows();
    int cur_window = current_window();
    auto sm = stats_array[cur_window];
    sm.second->add(a);
    update_write_timestamp(cur_window);
  }
  void add(const std::string& s) {
    std::lock_guard<std::mutex> lock(mutx);
    clean_windows();
    int cur_window = current_window();
    auto sm = stats_array[cur_window];
    sm.second->add(s);
    update_write_timestamp(cur_window);
  }
  void add(const std::string& s, int a) {
    std::lock_guard<std::mutex> lock(mutx);
    clean_windows();
    int cur_window = current_window();
    auto sm = stats_array[cur_window];
    sm.second->add(s, a);
    update_write_timestamp(cur_window);
  }
  void sub(int a) {
    std::lock_guard<std::mutex> lock(mutx);
    clean_windows();
    int cur_window = current_window();
    auto sm = stats_array[cur_window];
    sm.second->sub(a);
    update_write_timestamp(cur_window);
  }
  void sub(const std::string& s) {
    std::lock_guard<std::mutex> lock(mutx);
    clean_windows();
    int cur_window = current_window();
    auto sm = stats_array[cur_window];
    sm.second->sub(s);
    update_write_timestamp(cur_window);
  }
  int get() {
    std::lock_guard<std::mutex> lock(mutx);
    clean_windows();
    int cur_window = current_window();
    return stats_array[cur_window].second->get();
  }
  int get(const std::string& s) {
    std::lock_guard<std::mutex> lock(mutx);
    clean_windows();
    int cur_window = current_window();
    return stats_array[cur_window].second->get(s);
  }
  int get_current() { 
    std::lock_guard<std::mutex> lock(mutx);
    clean_windows();
    int cur_window = current_window();
    return stats_array[cur_window].second->get();
  }
  int get_current(const std::string& s) {
    std::lock_guard<std::mutex> lock(mutx);
    clean_windows();
    int cur_window = current_window();
    return stats_array[cur_window].second->get(s);    
  }
  void get_windows(std::vector<int>& ret_vec) {
    std::lock_guard<std::mutex> lock(mutx);
    clean_windows();
    int cur_window = current_window();
    for (int i=cur_window+num_windows; i>cur_window; i--) {
      int index = i % num_windows;
      ret_vec.push_back(stats_array[index].second->get());
    }
  }
  void get_windows(const std::string& s, std::vector<int>& ret_vec) {
    std::lock_guard<std::mutex> lock(mutx);
    clean_windows();
    int cur_window = current_window();
    for (int i=cur_window+num_windows; i>cur_window; i--) {
      int index = i % num_windows;
      ret_vec.push_back(stats_array[index].second->get(s));
    }
  }
  int sum() {
    std::lock_guard<std::mutex> lock(mutx);
    clean_windows();
    int cur_window = current_window();
    return stats_array[cur_window].second->sum(stats_array);
  }
  int sum(const std::string& s) {
    std::lock_guard<std::mutex> lock(mutx);
    clean_windows();
    int cur_window = current_window();
    return stats_array[cur_window].second->sum(s, stats_array);
  }
protected:
  void update_write_timestamp(int cur_window)
  {
    std::time_t now, write_time;
    std::time(&now);
    // only do this if the window first mod time is 0
    if (stats_array[cur_window].first == 0) {
      write_time = now - ((now - start_time) % window_size);
      stats_array[cur_window].first = write_time;
    }
  }
  int current_window() {
    std::time_t now, diff;
    std::time(&now);
    diff = now - start_time;
    int cw = (diff/window_size) % num_windows;
    return cw;
  }
  void clean_windows()
  {
    // this function checks the time difference since each array member was first written
    // and expires (zeros) any windows as necessary. This needs to be done before any data is read or written
    std::time_t now, expire_diff;
    std::time(&now);
    expire_diff = window_size * num_windows;

    // optimization - only clean windows if they need cleaning
    if ((now-last_cleaned) < window_size)
      return;

    for (TWStatsBuf::iterator i = stats_array.begin(); i != stats_array.end(); ++i) {
      std::time_t last_write = i->first;
      auto sm = i->second;
      if ((last_write != 0) && (now - last_write) >= expire_diff) {
	sm->erase();
	i->first = 0;
      }
    }
    last_cleaned = now;
  }
private:
  TWStatsBuf stats_array;
  mutable std::mutex mutx;
  int num_windows;
  int window_size;
  std::time_t start_time;
  std::time_t last_cleaned;
};

typedef std::shared_ptr<TWStatsEntry> TWStatsEntryP;

// key is field name, value is field type
typedef std::map<std::string, std::string> FieldMap;

const unsigned int ctwstats_map_size_soft = 524288;

template <typename T>
class TWStatsDB
{
public:
  typedef std::list<T> TWKeyTrackerType;
  explicit TWStatsDB(int w_siz, int num_w)
  {
    window_size = w_siz ? w_siz : 1;
    num_windows = num_w ? num_w : 1;
    std::time(&start_time);
    map_size_soft = ctwstats_map_size_soft;
  }

  static void twExpireThread(std::shared_ptr<TWStatsDB<std::string>> sdbp)
  {
    sdbp->expireEntries();
  }	
  void expireEntries();
  bool setFields(const FieldMap& fields);
  void incr(const T& key, const std::string& field_name);
  void decr(const T& key, const std::string& field_name);
  void add(const T& key, const std::string& field_name, int a); 
  void add(const T& key, const std::string& field_name, const std::string& s); 
  void add(const T& key, const std::string& field_name, const std::string& s, int a); 
  void sub(const T& key, const std::string& field_name, int a); 
  void sub(const T& key, const std::string& field_name, const std::string& s); 
  int get(const T& key, const std::string& field_name); // gets all fields summed/combined over all windows
  int get(const T& key, const std::string& field_name, const std::string& s); // gets all fields summed/combined over all windows for a particular value
  int get_current(const T& key, const std::string& field_name); // gets the value just for the current window
  int get_current(const T& key, const std::string& field_name, const std::string& s); // gets the value just for the current window for a particular value
  bool get_windows(const T& key, const std::string& field_name, std::vector<int>& ret_vec); // gets each window value returned in a vector
  bool get_windows(const T& key, const std::string& field_name, const std::string& s, std::vector<int>& ret_vec); // gets each window value returned in a vector for a particular value
  void set_map_size_soft(unsigned int size);
  unsigned int get_size();
protected:
  bool find_create_key_field(const T& key, const std::string& field_name, TWStatsEntryP& tp, typename TWKeyTrackerType::iterator* ktp, bool create=1);
  bool find_key_field(const T& key, const std::string& field_name, TWStatsEntryP& tp);  
  void update_write_timestamp(typename TWKeyTrackerType::iterator& kt)
  {
    // move this key to the end of the key tracker list
    key_tracker.splice(key_tracker.end(),
		       key_tracker,
		       kt);
  }
  int windowSize() { return window_size; }
  int numWindows() { return num_windows; }
private:
  TWKeyTrackerType key_tracker;
  typedef std::unordered_map<T, std::pair<typename TWKeyTrackerType::iterator, std::map<std::string, TWStatsEntryP>>> TWStatsDBMap;
  TWStatsDBMap stats_db;
  FieldMap field_map;
  int window_size;
  int num_windows;
  unsigned int map_size_soft;
  std::time_t start_time;
  mutable std::mutex mutx;
};

// Template methods
template <typename T>
bool TWStatsDB<T>::setFields(const FieldMap& fields)
{
  for (auto f : fields) {
    if (g_field_types.type_map.find(f.second) == g_field_types.type_map.end()) {
      return false;
    }
  }
  field_map = fields;
  return true;
}

template <typename T>
void TWStatsDB<T>::expireEntries()
{
  // spend some time every now and again expiring entries which haven't been updated 
  // wait at least window_size seconds before doing this each time
  unsigned int wait_interval = window_size;

  for (;;) {
    sleep(wait_interval);
    {
      std::lock_guard<std::mutex> lock(mutx);
  
      // don't bother expiring if the map isn't too big
      if (stats_db.size() <= map_size_soft)
	continue;

      unsigned int num_expire = stats_db.size() - map_size_soft;

      // this just uses the front of the key tracker list, which always contains the Least Recently Modified keys
      while (num_expire--) {
	const typename TWStatsDBMap::iterator it = stats_db.find(key_tracker.front());
	if (it != stats_db.end()) {
	  stats_db.erase(it);
	  key_tracker.pop_front();
	}
      }
    }
  }
}

template <typename T>
bool TWStatsDB<T>::find_key_field(const T& key, const std::string& field_name, TWStatsEntryP& tp)
{
  return find_create_key_field(key, field_name, tp, NULL, false);
}

template <typename T>
bool TWStatsDB<T>::find_create_key_field(const T& key, const std::string& field_name, TWStatsEntryP& tp, 
					 typename TWKeyTrackerType::iterator* keytrack, bool create)
{
  TWStatsBuf myrv;
  std::lock_guard<std::mutex> lock(mutx);

  // first check if the field name is in the field map - if not we throw the query out straight away
  auto myfield = field_map.find(field_name);
  if (myfield == field_map.end())
    return false;

  // Now we get the field type from the registered field types map
  auto mytype = g_field_types.type_map.find(myfield->second);
  if (mytype == g_field_types.type_map.end())
    return false;

  // otherwise look to see if the key and field name have been already inserted
  auto mysdb = stats_db.find(key);
  if (mysdb != stats_db.end()) {
    // the key already exists let's look for the field
    auto myfm = mysdb->second.second.find(field_name);
    if (myfm != mysdb->second.second.end()) {
      // awesome this key/field combination has already been created
      tp = myfm->second;
      if (keytrack)
	*keytrack = mysdb->second.first;
      return(true);
    }
    else {
      if (create) {
	// if we get here it means the key exists, but not the field so we need to add it
	auto field_type = mytype->first;
	tp = std::make_shared<TWStatsEntry>(num_windows, window_size, start_time, field_type);
	mysdb->second.second.insert(std::pair<std::string, TWStatsEntryP>(field_name, tp));
	if (keytrack)
	  *keytrack = mysdb->second.first;
	return(true);
      }
    }
  }
  else {
    if (create) {
      // if we get here, we have no key and no field
      auto field_type = mytype->first;
      tp = std::make_shared<TWStatsEntry>(num_windows, window_size, start_time, field_type);      
      std::map<std::string, TWStatsEntryP> myfm;
      myfm.insert(std::make_pair(field_name, tp));
      // add the key at the end of the key tracker list
      typename TWKeyTrackerType::iterator kit = key_tracker.insert(key_tracker.end(), key);
      stats_db.insert(std::make_pair(key, std::make_pair(kit, myfm)));
      if (keytrack)
	*keytrack = kit;
      return(true);
    }
  }
  return(false);
}

template <typename T>
void TWStatsDB<T>::incr(const T& key, const std::string& field_name)
{
  add(key, field_name, 1);
}

template <typename T>
void TWStatsDB<T>::decr(const T& key, const std::string& field_name)
{
  sub(key, field_name, 1);
}

template <typename T>
void TWStatsDB<T>::add(const T& key, const std::string& field_name, int a)
{
  typename TWKeyTrackerType::iterator kt;
  TWStatsEntryP tp;

  if (find_create_key_field(key, field_name, tp, &kt) != true) {
    return;
  }
  tp->add(a);
  update_write_timestamp(kt);
}

template <typename T>
void TWStatsDB<T>::add(const T& key, const std::string& field_name, const std::string& s)
{
  typename TWKeyTrackerType::iterator kt;
  TWStatsEntryP tp;

  if (find_create_key_field(key, field_name, tp, &kt) != true) {
    return;
  }
  tp->add(s);
  update_write_timestamp(kt);
}

template <typename T>
void TWStatsDB<T>::add(const T& key, const std::string& field_name, const std::string& s, int a)
{
  typename TWKeyTrackerType::iterator kt;
  TWStatsEntryP tp;

  if (find_create_key_field(key, field_name, tp, &kt) != true) {
    return;
  }
  tp->add(s, a);
  update_write_timestamp(kt);
}

template <typename T>
void TWStatsDB<T>::sub(const T& key, const std::string& field_name, int a)
{
  typename TWKeyTrackerType::iterator kt;
  TWStatsEntryP tp;

  if (find_create_key_field(key, field_name, tp, &kt) != true) {
    return;
  }
  tp->sub(a);
  update_write_timestamp(kt);
}

template <typename T>
void TWStatsDB<T>::sub(const T& key, const std::string& field_name, const std::string& s)
{
  typename TWKeyTrackerType::iterator kt;
  TWStatsEntryP tp;

  if (find_create_key_field(key, field_name, tp, &kt) != true) {
    return;
  }
  tp->sub(s);
  update_write_timestamp(kt);
}

template <typename T>
int TWStatsDB<T>::get(const T& key, const std::string& field_name)
{
  TWStatsEntryP tp;

  if (find_key_field(key, field_name, tp) != true) {
    return 0;
  }
  return tp->sum();
}

template <typename T>
int TWStatsDB<T>::get(const T& key, const std::string& field_name, const std::string& s)
{
  TWStatsEntryP tp;
  if (find_key_field(key, field_name, tp) != true) {
    return 0;
  }
  return tp->sum(s);
}

template <typename T>
int TWStatsDB<T>::get_current(const T& key, const std::string& field_name)
{
  TWStatsEntryP tp;
  if (find_key_field(key, field_name, tp) != true) {
    return 0;
  }
  return tp->get();
}

template <typename T>
int TWStatsDB<T>::get_current(const T& key, const std::string& field_name, const std::string& val)
{
  TWStatsEntryP tp;

  if (find_key_field(key, field_name, tp) != true) {
    return 0;
  }
  return tp->get(val);
}


template <typename T>
bool TWStatsDB<T>::get_windows(const T& key, const std::string& field_name, std::vector<int>& ret_vec)
{
  TWStatsEntryP tp;

  if (find_key_field(key, field_name, tp) != true) {
    return false;
  }
  tp->get_windows(ret_vec);
  return(true);
}

template <typename T>
bool TWStatsDB<T>::get_windows(const T& key, const std::string& field_name, const std::string& val, std::vector<int>& ret_vec)
{
  TWStatsEntryP tp;

  if (find_key_field(key, field_name, tp) != true) {
    return false;
  }
  tp->get_windows(val, ret_vec);
  return(true);
}

template <typename T>
void TWStatsDB<T>::set_map_size_soft(unsigned int size) 
{
  std::lock_guard<std::mutex> lock(mutx);

  map_size_soft = size;
}

template <typename T>
unsigned int TWStatsDB<T>::get_size()
{
  std::lock_guard<std::mutex> lock(mutx);

  return stats_db.size();
}

typedef boost::variant<std::string, int, ComboAddress> TWKeyType;

// This is a Lua-friendly wrapper to the Stats DB
class TWStringStatsDBWrapper
{
public:
  std::shared_ptr<TWStatsDB<std::string>> sdbp;
  uint8_t v4_prefix=32;
  uint8_t v6_prefix=128;

  TWStringStatsDBWrapper(int window_size, int num_windows)
  {
    sdbp = std::make_shared<TWStatsDB<std::string>>(window_size, num_windows);
    thread t(TWStatsDB<std::string>::twExpireThread, sdbp);
    t.detach();
  }

  TWStringStatsDBWrapper(int window_size, int num_windows, const std::vector<pair<std::string, std::string>>& fmvec)
  {
    sdbp = std::make_shared<TWStatsDB<std::string>>(window_size, num_windows);    
    (void)setFields(fmvec);
    thread t(TWStatsDB<std::string>::twExpireThread, sdbp);
    t.detach();
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
    v4_prefix = bits > 32 ? 32 : bits;
  }

  void setv6Prefix(uint8_t bits)
  {
    v6_prefix = bits > 128 ? 128 : bits;
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
	return Netmask(ca, v4_prefix).toStringNetwork();
      }
      else if (ca.isIpv6()) {
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

  unsigned int get_size()
  {	
    return sdbp->get_size();
  }

  void set_size_soft(unsigned int size) 
  {
    sdbp->set_map_size_soft(size);
  }

};
  
