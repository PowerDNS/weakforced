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
#include "ext/hyperloglog.hpp"
#include "ext/count_min_sketch.hpp"
#include "iputils.hh"

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

class TWStatsMemberInt;

typedef std::shared_ptr<TWStatsMemberInt> TWStatsMemberIntP;

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
  void add(const std::string& s) { return; }
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

// key is field name, value is field type
typedef std::map<std::string, std::string> FieldMap;

template <typename T>
class TWStatsDB
{
public:
  explicit TWStatsDB(int w_siz, int num_w)
  {
    window_size = w_siz ? w_siz : 1;
    num_windows = num_w ? num_w : 1;
    std::time(&start_time);
    
  }
  bool setFields(const FieldMap& fields);
  int incr(const T& key, const std::string& field_name);
  int decr(const T& key, const std::string& field_name);
  int add(const T& key, const std::string& field_name, int a); // returns all fields summed/combined
  int add(const T& key, const std::string& field_name, const std::string& s); // returns all fields summed/combined
  int add(const T& key, const std::string& field_name, const std::string& s, int a); // returns all fields summed/combined
  int sub(const T& key, const std::string& field_name, int a); // returns all fields summed/combined
  int sub(const T& key, const std::string& field_name, const std::string& s); // returns all fields summed/combined
  int get(const T& key, const std::string& field_name); // gets all fields summed/combined over all windows
  int get(const T& key, const std::string& field_name, const std::string& s); // gets all fields summed/combined over all windows for a particular value
  int get_current(const T& key, const std::string& field_name); // gets the value just for the current window
  int get_current(const T& key, const std::string& field_name, const std::string& s); // gets the value just for the current window for a particular value
  bool get_windows(const T& key, const std::string& field_name, std::vector<int>& ret_vec); // gets each window value returned in a vector
  bool get_windows(const T& key, const std::string& field_name, const std::string& s, std::vector<int>& ret_vec); // gets each window value returned in a vector for a particular value
  void set(const T& key, const std::string& field_name, int a);
  void set(const T& key, const std::string& field_name, const std::string& s);
protected:
  bool find_create_key_field(const T& key, const std::string& field_name, TWStatsBuf*& ret_vec, bool create=1);
  bool find_key_field(const T& key, const std::string& field_name, TWStatsBuf*& ret_vec);  
  int current_window() {
    std::time_t now, diff;
    std::time(&now);
    diff = now - start_time;
    int cw = (diff/window_size) % num_windows;
    return cw;
  }
  void update_write_timestamp(std::pair<std::time_t, TWStatsMemberP>& vec)
  {
    // only do this if the window first mod time is 0
    if (vec.first == 0) {
      std::time_t now, write_time;
      std::time(&now);
      write_time = now - ((now - start_time) % window_size);
      vec.first = write_time;
    }
  }
  void clean_windows(TWStatsBuf& twsbuf);
private:
  typedef std::unordered_map<T, std::map<std::string, TWStatsBuf>> TWStatsDBMap;
  TWStatsDBMap stats_db;
  FieldMap field_map;
  int window_size;
  int num_windows;
  int cur_window = 0;
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

// XXX this function probably needs to move into a background thread housekeeping task
// XXX Since I use a circular buffer, it should run at least every window_size seconds
// XXX If the class was changed to one which just pushed new windows onto the front, we could expire
// XXX windows off the back in our own time rather than have the restriction of window_size
template <typename T>
void TWStatsDB<T>::clean_windows(TWStatsBuf& twsb)
{
  // this function checks the time difference since each array member was first written
  // and expires (zeros) any windows as necessary. This needs to be done before any data is read or written
  std::time_t now, expire_diff;
  std::time(&now);
  expire_diff = window_size * num_windows;

  for (TWStatsBuf::iterator i = twsb.begin(); i != twsb.end(); ++i) {
    std::time_t last_write = i->first;
    auto sm = i->second;
    if ((last_write != 0) && (now - last_write) >= expire_diff) {
      sm->erase();
      i->first = 0;
    }
  }
}

template <typename T>
bool TWStatsDB<T>::find_key_field(const T& key, const std::string& field_name, TWStatsBuf*& ret_vec)
{
  return find_create_key_field(key, field_name, ret_vec, false);
}

template <typename T>
bool TWStatsDB<T>::find_create_key_field(const T& key, const std::string& field_name, TWStatsBuf*& ret_vec, 
					 bool create)
{
  TWStatsBuf myrv;
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
    auto myfm = mysdb->second.find(field_name);
    if (myfm != mysdb->second.end()) {
      // awesome this key/field combination has already been created
      ret_vec = &(myfm->second);
      // whenever we retrieve a set of windows, we clean them to remove expired windows
      clean_windows(myfm->second);
      cur_window = current_window();
      return(true);
    }
    else {
      if (create) {
	// if we get here it means the key exists, but not the field so we need to add it
	auto mystat = mytype->second;
	for (int i=0; i< num_windows; i++) {
	  myrv.push_back(std::pair<std::time_t, TWStatsMemberP>((std::time_t)0, mystat()));
	}
	mysdb->second.insert(std::pair<std::string, TWStatsBuf>(field_name, myrv));
      }
    }
  }
  else {
    if (create) {
      // if we get here, we have no key and no field
      auto mystat = mytype->second;
      for (int i=0; i< num_windows; i++) {
	myrv.push_back(std::pair<std::time_t, TWStatsMemberP>((std::time_t)0, mystat()));
      }
      std::map<std::string, TWStatsBuf> myfm;
      myfm.insert(std::pair<std::string, TWStatsBuf>(field_name, myrv));
      stats_db.insert(std::pair<T, std::map<std::string, TWStatsBuf>>(key, myfm));
    }
  }
  // update the current window
  cur_window = current_window();
  // we created the field, now just look it up again so we get the actual pointer not a copy
  if (create)
    return (find_create_key_field(key, field_name, ret_vec, false));
  else
    return(false);
}

template <typename T>
int TWStatsDB<T>::incr(const T& key, const std::string& field_name)
{
  return add(key, field_name, 1);
}

template <typename T>
int TWStatsDB<T>::decr(const T& key, const std::string& field_name)
{
  return sub(key, field_name, 1);
}

template <typename T>
int TWStatsDB<T>::add(const T& key, const std::string& field_name, int a)
{
  TWStatsBuf* myvec;
  std::lock_guard<std::mutex> lock(mutx);

  if (find_create_key_field(key, field_name, myvec) != true) {
    return 0;
  }
  auto sm = (*myvec)[cur_window];

  sm.second->add(a);
  update_write_timestamp((*myvec)[cur_window]);
  return sm.second->sum(*myvec);
}

template <typename T>
int TWStatsDB<T>::add(const T& key, const std::string& field_name, const std::string& s)
{
  TWStatsBuf* myvec;
  std::lock_guard<std::mutex> lock(mutx);

  if (find_create_key_field(key, field_name, myvec) != true) {
    return 0;
  }
  auto sm = (*myvec)[cur_window];

  sm.second->add(s);
  update_write_timestamp((*myvec)[cur_window]);
  return sm.second->sum(*myvec);
}

template <typename T>
int TWStatsDB<T>::add(const T& key, const std::string& field_name, const std::string& s, int a)
{
  TWStatsBuf* myvec;
  std::lock_guard<std::mutex> lock(mutx);

  if (find_create_key_field(key, field_name, myvec) != true) {
    return 0;
  }
  auto sm = (*myvec)[cur_window];

  sm.second->add(s, a);
  update_write_timestamp((*myvec)[cur_window]);
  return sm.second->sum(s, *myvec);
}

template <typename T>
int TWStatsDB<T>::sub(const T& key, const std::string& field_name, int a)
{
  TWStatsBuf* myvec;
  std::lock_guard<std::mutex> lock(mutx);

  if (find_create_key_field(key, field_name, myvec) != true) {
    return 0;
  }
  auto sm = (*myvec)[cur_window];

  sm.second->sub(a);
  update_write_timestamp((*myvec)[cur_window]);
  return sm.second->sum(*myvec);
}

template <typename T>
int TWStatsDB<T>::sub(const T& key, const std::string& field_name, const std::string& s)
{
  TWStatsBuf* myvec;
  std::lock_guard<std::mutex> lock(mutx);

  if (find_create_key_field(key, field_name, myvec) != true) {
    return 0;
  }
  auto sm = (*myvec)[cur_window];

  sm.second->sub(s);
  update_write_timestamp((*myvec)[cur_window]);
  return sm.second->sum(*myvec);
}

template <typename T>
int TWStatsDB<T>::get(const T& key, const std::string& field_name)
{
  TWStatsBuf* myvec;
  std::lock_guard<std::mutex> lock(mutx);

  if (find_key_field(key, field_name, myvec) != true) {
    return 0;
  }
  auto sm = (*myvec)[cur_window];

  return sm.second->sum(*myvec);
}

template <typename T>
int TWStatsDB<T>::get(const T& key, const std::string& field_name, const std::string& s)
{
  TWStatsBuf* myvec;
  std::lock_guard<std::mutex> lock(mutx);

  if (find_key_field(key, field_name, myvec) != true) {
    return 0;
  }
  auto sm = (*myvec)[cur_window];

  return sm.second->sum(s, *myvec);
}


template <typename T>
int TWStatsDB<T>::get_current(const T& key, const std::string& field_name)
{
  TWStatsBuf* myvec;
  std::lock_guard<std::mutex> lock(mutx);

  if (find_key_field(key, field_name, myvec) != true) {
    return 0;
  }
  auto sm = (*myvec)[cur_window];

  return sm.second->get();
}

template <typename T>
int TWStatsDB<T>::get_current(const T& key, const std::string& field_name, const std::string& val)
{
  TWStatsBuf* myvec;
  std::lock_guard<std::mutex> lock(mutx);

  if (find_key_field(key, field_name, myvec) != true) {
    return 0;
  }
  auto sm = (*myvec)[cur_window];

  return sm.second->get(val);
}


template <typename T>
bool TWStatsDB<T>::get_windows(const T& key, const std::string& field_name, std::vector<int>& ret_vec)
{
  TWStatsBuf* myvec;
  std::lock_guard<std::mutex> lock(mutx);

  if (find_key_field(key, field_name, myvec) != true) {
    return false;
  }
  for (auto sm : (*myvec)) {
    ret_vec.push_back(sm.second->get());
  }

  return(true);
}

template <typename T>
bool TWStatsDB<T>::get_windows(const T& key, const std::string& field_name, const std::string& val, std::vector<int>& ret_vec)
{
  TWStatsBuf* myvec;
  std::lock_guard<std::mutex> lock(mutx);

  if (find_key_field(key, field_name, myvec) != true) {
    return false;
  }
  for (auto sm : (*myvec)) {
    ret_vec.push_back(sm.second->get(val));
  }

  return(true);
}

typedef boost::variant<std::string, int, ComboAddress> TWKeyType;

// This is a Lua-friendly wrapper to the Stats DB
class TWStringStatsDBWrapper
{
public:
  std::shared_ptr<TWStatsDB<std::string>> sdbp;

  TWStringStatsDBWrapper(int window_size, int num_windows)
  {
    sdbp = std::make_shared<TWStatsDB<std::string>>(window_size, num_windows);
  }

  TWStringStatsDBWrapper(int window_size, int num_windows, const std::vector<pair<std::string, std::string>>& fmvec)
  {
    sdbp = std::make_shared<TWStatsDB<std::string>>(window_size, num_windows);
    (void)setFields(fmvec);
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

  std::string getStringKey(const TWKeyType vkey)
  {
    if (vkey.which() == 0) {
      return boost::get<std::string>(vkey);
    }
    else if (vkey.which() == 1) {
      return std::to_string(boost::get<int>(vkey));
    }
    else if (vkey.which() == 2) {
      return boost::get<ComboAddress>(vkey).toString();
    }
    else
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

  void sub(const std::string& key, const std::string& field_name, const boost::variant<std::string, int>& val)
  {
    if (val.which() == 0) {
      std::string mystr = boost::get<std::string>(val);
      sdbp->sub(key, field_name, mystr);
    }
    else {
      int myint = boost::get<int>(val);
      sdbp->sub(key, field_name, myint);
    }
  }
  
  int get(const std::string& key, const std::string& field_name, const boost::optional<boost::variant<std::string, ComboAddress>> param1)
  {
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

  int get_current(const std::string& key, const std::string& field_name, const boost::optional<boost::variant<std::string, ComboAddress>> param1)
  {
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

  std::vector<int> get_windows(const std::string& key, const std::string& field_name, const boost::optional<boost::variant<std::string, ComboAddress>> param1)
  {
    std::vector<int> retvec;
    
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

};
  
