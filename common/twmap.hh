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
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <ctime>
#include <atomic>
#include <mutex>
#include <thread>
#include <boost/variant.hpp>
#include <boost/optional.hpp>
#include <iostream>
#include <sstream>
#include "ext/hyperloglog.hpp"
#include "ext/count_min_sketch.hpp"
#include "iputils.hh"
#include "dolog.hh"
#include "wforce_ns.hh"

using std::thread;

class TWStatsMember;

typedef std::unique_ptr<TWStatsMember> TWStatsMemberP;
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
  TWStatsMemberInt() {}
  TWStatsMemberInt(const TWStatsMemberInt&) = delete;
  TWStatsMemberInt& operator=(const TWStatsMemberInt&) = delete;
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
    for (auto a=vec.begin(); a!=vec.end(); ++a)
      {
        count += a->second->get();
      }
    return count;
  }
  int sum(const std::string& s, const TWStatsBuf& vec) { return 0; }
private:
  int i=0;
};

#define HLL_NUM_REGISTER_BITS 6

class TWStatsMemberHLL : public TWStatsMember
{
public:
  TWStatsMemberHLL()
  {
    hllp = wforce::make_unique<hll::HyperLogLog>(HLL_NUM_REGISTER_BITS);
  }
  TWStatsMemberHLL(const TWStatsMemberHLL&) = delete;
  TWStatsMemberHLL& operator=(const TWStatsMemberHLL&) = delete;
  void add(int a) { std::string str; str = std::to_string(a); hllp->add(str.c_str(), str.length()); return; }
  void add(const std::string& s) { hllp->add(s.c_str(), s.length()); }
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
    for (auto a = vec.begin(); a != vec.end(); ++a)
      {
	// XXX yes not massively pretty
	hllsum.merge(*((dynamic_cast<TWStatsMemberHLL&>(*(a->second))).hllp));
      }
    return std::lround(hllsum.estimate());
  }
  int sum(const std::string& s, const TWStatsBuf& vec) { return 0; }
private:
  std::unique_ptr<hll::HyperLogLog> hllp;
};

#define COUNTMIN_EPS 0.05
#define COUNTMIN_GAMMA 0.2

class TWStatsMemberCountMin : public TWStatsMember
{
public:
  TWStatsMemberCountMin()
  {
    cm = wforce::make_unique<CountMinSketch>(COUNTMIN_EPS, COUNTMIN_GAMMA);
  }
  TWStatsMemberCountMin(const TWStatsMemberCountMin&) = delete;
  TWStatsMemberCountMin& operator=(const TWStatsMemberCountMin&) = delete;
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
    for (auto a=vec.begin(); a!=vec.end(); ++a)
      {
	count += a->second->get();
      }
    return count;
  }
  int sum(const std::string&s, const TWStatsBuf& vec)
  {
    int count = 0;
    for (auto a=vec.begin(); a!=vec.end(); ++a)
      {
	count += a->second->get(s);
      }
    return count;
  }
private:
  std::unique_ptr<CountMinSketch> cm;
};

template<typename T> TWStatsMemberP createInstance() { return wforce::make_unique<T>(); }

typedef std::map<std::string, TWStatsMemberP(*)()> map_type;

// The idea here is that the type mechanism is extensible, e.g. could add a bloom filter etc.
// XXX Should make this dynamically extensible
class TWStatsTypeMap
{
public:
  map_type type_map;
  static TWStatsTypeMap& getInstance()
  {
    // this is thread-safe in C++11
    static TWStatsTypeMap myInstance;
    return myInstance;
  }
  TWStatsTypeMap(const TWStatsTypeMap& src) = delete; // copy construct
  TWStatsTypeMap(TWStatsTypeMap&&) = delete; // move construct
  TWStatsTypeMap& operator=(const TWStatsTypeMap& rhs) = delete; // copy assign
  TWStatsTypeMap& operator=(TWStatsTypeMap &&) = delete; // move assign
  
protected:
  TWStatsTypeMap()
  {
    type_map["int"] = &createInstance<TWStatsMemberInt>;
    type_map["hll"] = &createInstance<TWStatsMemberHLL>;
    type_map["countmin"] = &createInstance<TWStatsMemberCountMin>;
  }
};

// this class is not protected by mutexes because it sits behind TWStatsDB which has a mutex
// controlling all access
class TWStatsEntry
{
public:
  TWStatsEntry(int nw, int ws, std::time_t st, const std::string field_type) {
    num_windows = nw;
    start_time = st;
    window_size = ws;
    last_cleaned = 0;
    auto& field_types = TWStatsTypeMap::getInstance();
    
    auto it = field_types.type_map.find(field_type);
    if (it != field_types.type_map.end()) {
      for (int i=0; i< num_windows; i++) {
	stats_array.push_back(std::pair<std::time_t, TWStatsMemberP>((std::time_t)0, it->second()));
      }
    }
  }

  // prevent copies at compile time
  TWStatsEntry(const TWStatsEntry&) = delete;
  TWStatsEntry& operator=(const TWStatsEntry&) = delete;

  void add(int a) {
    clean_windows();
    int cur_window = current_window();
    auto& sm = stats_array[cur_window];
    sm.second->add(a);
    update_write_timestamp(cur_window);
  }
  void add(const std::string& s) {
    clean_windows();
    int cur_window = current_window();
    auto& sm = stats_array[cur_window];
    sm.second->add(s);
    update_write_timestamp(cur_window);
  }
  void add(const std::string& s, int a) {
    clean_windows();
    int cur_window = current_window();
    auto& sm = stats_array[cur_window];
    sm.second->add(s, a);
    update_write_timestamp(cur_window);
  }
  void sub(int a) {
    clean_windows();
    int cur_window = current_window();
    auto& sm = stats_array[cur_window];
    sm.second->sub(a);
    update_write_timestamp(cur_window);
  }
  void sub(const std::string& s) {
    clean_windows();
    int cur_window = current_window();
    auto& sm = stats_array[cur_window];
    sm.second->sub(s);
    update_write_timestamp(cur_window);
  }
  int get() {
    clean_windows();
    int cur_window = current_window();
    return stats_array[cur_window].second->get();
  }
  int get(const std::string& s) {
    clean_windows();
    int cur_window = current_window();
    return stats_array[cur_window].second->get(s);
  }
  int get_current() { 
    clean_windows();
    int cur_window = current_window();
    return stats_array[cur_window].second->get();
  }
  int get_current(const std::string& s) {
    clean_windows();
    int cur_window = current_window();
    return stats_array[cur_window].second->get(s);    
  }
  void get_windows(std::vector<int>& ret_vec) {
    clean_windows();
    int cur_window = current_window();
    for (int i=cur_window+num_windows; i>cur_window; i--) {
      int index = i % num_windows;
      ret_vec.push_back(stats_array[index].second->get());
    }
  }
  void get_windows(const std::string& s, std::vector<int>& ret_vec) {
    clean_windows();
    int cur_window = current_window();
    for (int i=cur_window+num_windows; i>cur_window; i--) {
      int index = i % num_windows;
      ret_vec.push_back(stats_array[index].second->get(s));
    }
  }
  int sum() {
    clean_windows();
    int cur_window = current_window();
    if (sum_cache_valid == true)
      return sum_cache_value;
    else {
      sum_cache_value = stats_array[cur_window].second->sum(stats_array);
      return sum_cache_value;
    }
  }
  int sum(const std::string& s) {
    clean_windows();
    int cur_window = current_window();
    if (ssum_cache_valid == true)
      return ssum_cache_value;
    else {
      ssum_cache_value = stats_array[cur_window].second->sum(s, stats_array);
      return ssum_cache_value;
    }
  }
  void reset() {
    for (TWStatsBuf::iterator i = stats_array.begin(); i != stats_array.end(); ++i) {
      i->second->erase();
      i->first = 0;
    }
    sum_cache_valid = false;
    ssum_cache_valid = false;
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
    sum_cache_valid = false;
    ssum_cache_valid = false;
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
      if ((last_write != 0) && (now - last_write) >= expire_diff) {
	i->second->erase();
	i->first = 0;
	sum_cache_valid = false;
	ssum_cache_valid = false;
      }
    }
    last_cleaned = now;
  }
private:
  TWStatsBuf stats_array;
  int num_windows;
  int window_size;
  std::time_t start_time;
  std::time_t last_cleaned;
  bool sum_cache_valid = false;
  int sum_cache_value = 0;
  bool ssum_cache_valid = false;
  int ssum_cache_value = 0;
};

typedef std::unique_ptr<TWStatsEntry> TWStatsEntryP;

// key is field name, value is field type
typedef std::map<std::string, std::string> FieldMap;

const unsigned int ctwstats_map_size_soft = 524288;

template <typename T>
class TWStatsDB
{
public:
  typedef std::list<T> TWKeyTrackerType;
  explicit TWStatsDB(const std::string& name, int w_siz, int num_w)
  {
    window_size = w_siz ? w_siz : 1;
    num_windows = num_w ? num_w : 1;
    std::time(&start_time);
    map_size_soft = ctwstats_map_size_soft;
    db_name = name;
  }
  // detect attempts to copy at compile time
  TWStatsDB(const TWStatsDB&) = delete;
  TWStatsDB& operator=(const TWStatsDB&) = delete;
  
  std::string getDBName()
  {
    return db_name;
  }
  
  static void twExpireThread(std::shared_ptr<TWStatsDB<std::string>> sdbp)
  {
    sdbp->expireEntries();
  }	
  void expireEntries();
  bool setFields(const FieldMap& fields);
  const FieldMap& getFields() {
    return field_map;
  }
  void incr(const T& key, const std::string& field_name);
  void decr(const T& key, const std::string& field_name);
  void add(const T& key, const std::string& field_name, int a); 
  void add(const T& key, const std::string& field_name, const std::string& s); 
  void add(const T& key, const std::string& field_name, const std::string& s, int a); 
  void sub(const T& key, const std::string& field_name, int a); 
  void sub(const T& key, const std::string& field_name, const std::string& s); 
  int get(const T& key, const std::string& field_name); // gets all values summed/combined over all windows
  int get(const T& key, const std::string& field_name, const std::string& s); // gets all values summed/combined over all windows for a particular value
  int get_current(const T& key, const std::string& field_name); // gets the value just for the current window
  int get_current(const T& key, const std::string& field_name, const std::string& s); // gets the value just for the current window for a particular value
  bool get_windows(const T& key, const std::string& field_name, std::vector<int>& ret_vec); // gets each window value returned in a vector
  bool get_windows(const T& key, const std::string& field_name, const std::string& s, std::vector<int>& ret_vec); // gets each window value returned in a vector for a particular value
  bool get_all_fields(const T& key,  std::vector<std::pair<std::string, int>>& ret_vec);
  void reset(const T&key); // Reset to zero all fields for a given key
  void set_map_size_soft(unsigned int size);
  unsigned int get_size();
  unsigned int get_max_size();
  uint8_t getv4Prefix() { return v4_prefix; }
  uint8_t getv6Prefix() { return v6_prefix; }
  void setv4Prefix(uint8_t prefix) { v4_prefix = prefix; }
  void setv6Prefix(uint8_t prefix) { v6_prefix = prefix; }
  int windowSize() { return window_size; }
  int numWindows() { return num_windows; }
protected:
  template<typename Fn>
  bool find_create_key_field(const T& key, const std::string& field_name,
					   Fn fn);
  template<typename Fn>
  bool find_key_field(const T& key, const std::string& field_name,
					   Fn fn);
  template<typename Fn>
  bool _find_create_key_field(const T& key, const std::string& field_name,
					   Fn fn, bool create);

  void update_write_timestamp(typename TWKeyTrackerType::iterator& kt)
  {
    // this is always called from a mutex lock (or should be)
    // move this key to the end of the key tracker list
    key_tracker.splice(key_tracker.end(),
		       key_tracker,
		       kt);
  }
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
  std::string db_name;
  uint8_t v4_prefix=32;
  uint8_t v6_prefix=128;
};

// Template methods
template <typename T>
bool TWStatsDB<T>::setFields(const FieldMap& fields)
{
  auto& field_types = TWStatsTypeMap::getInstance();
  for (auto f : fields) {
    if (field_types.type_map.find(f.second) == field_types.type_map.end()) {
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
  unsigned int wait_interval = window_size < 30 ? window_size : 30;

  for (;;) {
    sleep(wait_interval);
    {
      std::lock_guard<std::mutex> lock(mutx);
  
      // don't bother expiring if the map isn't too big
      if (stats_db.size() <= map_size_soft)
	continue;

      unsigned int num_expire = stats_db.size() - map_size_soft;
      unsigned int num_expired = num_expire;

      infolog("About to expire %d entries from stats db %s", num_expire, db_name);

      // this just uses the front of the key tracker list, which always contains the Least Recently Modified keys
      while (num_expire--) {
	const typename TWStatsDBMap::iterator it = stats_db.find(key_tracker.front());
	if (it != stats_db.end()) {
	  stats_db.erase(it);
	  key_tracker.pop_front();
	}
      }
      infolog("Finished expiring %d entries from stats db %s", num_expired, db_name);
    }
  }
}

template <typename T>
template <typename Fn>
bool TWStatsDB<T>::find_key_field(const T& key, const std::string& field_name, Fn fn)
{
  std::lock_guard<std::mutex> lock(mutx);
  return _find_create_key_field(key, field_name, fn, false);
}

template <typename T>
template <typename Fn>
bool TWStatsDB<T>::find_create_key_field(const T& key, const std::string& field_name,
					 Fn fn)
{
  std::lock_guard<std::mutex> lock(mutx);
  return _find_create_key_field(key, field_name, fn, true);
}

template <typename T>
template <typename Fn>
bool TWStatsDB<T>::_find_create_key_field(const T& key, const std::string& field_name,
					  Fn fn, bool create)
{
  TWStatsBuf myrv;
  bool retval = false;

  // first check if the field name is in the field map - if not we throw the query out straight away
  auto myfield = field_map.find(field_name);
  if (myfield == field_map.end())
    return false;

  // Now we get the field type from the registered field types map
  auto& field_types = TWStatsTypeMap::getInstance();
  auto mytype = field_types.type_map.find(myfield->second);
  if (mytype == field_types.type_map.end())
    return false;

  // otherwise look to see if the key and field name have been already inserted
  auto mysdb = stats_db.find(key);
  if (mysdb != stats_db.end()) {
    // the key already exists let's look for the field
    auto myfm = mysdb->second.second.find(field_name);
    if (myfm != mysdb->second.second.end()) {
      // awesome this key/field combination has already been created so call the supplied lambda
      fn(myfm, mysdb->second.first);
      retval = true;
    }
    else {
      if (create) {
	// if we get here it means the key exists, but not the field so we need to add it
	mysdb->second.second.emplace(std::make_pair(field_name, wforce::make_unique<TWStatsEntry>(num_windows, window_size, start_time, mytype->first)));
	retval = _find_create_key_field(key, field_name, fn, false);
      }
    }
  }
  else {
    if (create) {
      // if we get here, we have no key and no field
      // add the key at the end of the key tracker list
      typename TWKeyTrackerType::iterator kit = key_tracker.insert(key_tracker.end(), key);
      std::map<std::string, TWStatsEntryP> myfm;
      myfm.emplace(std::make_pair(field_name, wforce::make_unique<TWStatsEntry>(num_windows, window_size, start_time, mytype->first)));
      stats_db.emplace(std::make_pair(key, std::make_pair(kit, std::move(myfm))));
      retval = _find_create_key_field(key, field_name, fn, false);
    }
  }
  return(retval);
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
  find_create_key_field(key, field_name, [a, this](std::map<std::string, TWStatsEntryP>::iterator it, typename TWKeyTrackerType::iterator& kt)
			    {
			      it->second->add(a);
			      this->update_write_timestamp(kt);
			    });
}

template <typename T>
void TWStatsDB<T>::add(const T& key, const std::string& field_name, const std::string& s)
{
  find_create_key_field(key, field_name, [s, this](std::map<std::string, TWStatsEntryP>::iterator it, typename TWKeyTrackerType::iterator& kt)
			{
			  it->second->add(s);
			  this->update_write_timestamp(kt);
			});
}

template <typename T>
void TWStatsDB<T>::add(const T& key, const std::string& field_name, const std::string& s, int a)
{
  find_create_key_field(key, field_name, [&s, a, this](std::map<std::string, TWStatsEntryP>::iterator it, typename TWKeyTrackerType::iterator& kt)
			{
			  it->second->add(s, a);
			  this->update_write_timestamp(kt);
			});
}

template <typename T>
void TWStatsDB<T>::sub(const T& key, const std::string& field_name, int a)
{
  find_create_key_field(key, field_name, [a, this](std::map<std::string, TWStatsEntryP>::iterator it, typename TWKeyTrackerType::iterator& kt)
			{
			  it->second->sub(a);
			  update_write_timestamp(kt);
			});
}

template <typename T>
void TWStatsDB<T>::sub(const T& key, const std::string& field_name, const std::string& s)
{
  find_create_key_field(key, field_name, [s, this](std::map<std::string, TWStatsEntryP>::iterator it, typename TWKeyTrackerType::iterator& kt)
			{
			  it->second->sub(s);
			  update_write_timestamp(kt);
			});
}

template <typename T>
int TWStatsDB<T>::get(const T& key, const std::string& field_name)
{
  int retval=0;
  
  find_key_field(key, field_name, [&retval](std::map<std::string, TWStatsEntryP>::iterator it, typename TWKeyTrackerType::iterator& kt)
		 {
		   retval = it->second->sum();
		 });
  return retval;
}

template <typename T>
int TWStatsDB<T>::get(const T& key, const std::string& field_name, const std::string& s)
{
  int retval=0;
  
  find_key_field(key, field_name, [&retval, s](std::map<std::string, TWStatsEntryP>::iterator it, typename TWKeyTrackerType::iterator& kt)
		 {
		   retval = it->second->sum(s);
		 });
  return retval;
}

template <typename T>
int TWStatsDB<T>::get_current(const T& key, const std::string& field_name)
{
  int retval=0;
  
  find_key_field(key, field_name, [&retval](std::map<std::string, TWStatsEntryP>::iterator it, typename TWKeyTrackerType::iterator& kt)
		 {
		   retval = it->second->get();
		 });
  return retval;
}

template <typename T>
int TWStatsDB<T>::get_current(const T& key, const std::string& field_name, const std::string& val)
{
  int retval=0;
  
  find_key_field(key, field_name, [&retval, val](std::map<std::string, TWStatsEntryP>::iterator it, typename TWKeyTrackerType::iterator& kt)
		 {
		   retval = it->second->get(val);
		 });
  return retval;
}


template <typename T>
bool TWStatsDB<T>::get_windows(const T& key, const std::string& field_name, std::vector<int>& ret_vec)
{
  bool retval=false;
  
  retval = find_key_field(key, field_name, [&ret_vec](std::map<std::string, TWStatsEntryP>::iterator it, typename TWKeyTrackerType::iterator& kt)
		 {
		   it->second->get_windows(ret_vec);
		 });
  return(retval);
}

template <typename T>
bool TWStatsDB<T>::get_windows(const T& key, const std::string& field_name, const std::string& val, std::vector<int>& ret_vec)
{
  bool retval=false;
  
  retval = find_key_field(key, field_name, [&val, &ret_vec](std::map<std::string, TWStatsEntryP>::iterator it, typename TWKeyTrackerType::iterator& kt)
		 {
		   it->second->get_windows(val, ret_vec);
		 });
  return(retval);
}

template <typename T>
bool TWStatsDB<T>::get_all_fields(const T& key, std::vector<std::pair<std::string, int>>& ret_vec)
{
  std::lock_guard<std::mutex> lock(mutx);  

  auto mysdb = stats_db.find(key);
  if (mysdb == stats_db.end()) {
    return false;
  }
  else {
    auto& myfm = mysdb->second.second;
    // go through all the fields, get them and add to a vector
    for (auto it = myfm.begin(); it != myfm.end(); ++it) {
      ret_vec.push_back(make_pair(it->first, it->second->sum()));
    }
  }
  return true;
}

template <typename T>
void TWStatsDB<T>::reset(const T& key)
{
  std::lock_guard<std::mutex> lock(mutx);  

  auto mysdb = stats_db.find(key);
  if (mysdb != stats_db.end()) {
    auto& myfm = mysdb->second.second;
    // go through all the fields and reset them
    for (auto it = myfm.begin(); it != myfm.end(); ++it) {
      it->second->reset();
    }
  }
}

template <typename T>
void TWStatsDB<T>::set_map_size_soft(unsigned int size) 
{
  std::lock_guard<std::mutex> lock(mutx);

  map_size_soft = size;
  stats_db.reserve(size);
}

template <typename T>
unsigned int TWStatsDB<T>::get_size()
{
  std::lock_guard<std::mutex> lock(mutx);

  return stats_db.size();
}

template <typename T>
unsigned int TWStatsDB<T>::get_max_size()
{
  std::lock_guard<std::mutex> lock(mutx);

  return map_size_soft;
}

