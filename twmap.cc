#include "twmap.hh"
#include <iostream>
#include <unistd.h>

static TWStatsTypeMap g_field_types;

template <typename T>
bool TWStatsDB<T>::setFields(std::shared_ptr<FieldMap> fieldsp)
{
  for (auto f : *fieldsp) {
    if (g_field_types.type_map.find(f.second) == g_field_types.type_map.end()) {
      return false;
    }
  }
  field_map = fieldsp; // copy - only small
  return true;
}

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
  auto myfield = field_map->find(field_name);
  if (myfield == field_map->end())
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
  return (find_create_key_field(key, field_name, ret_vec, false));
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

#include <string.h>
#include "hyperloglog.hpp"

main (int argc, char** argv)
{
  TWStatsDB<std::string> sdb(1,60);
  auto field_map = std::make_shared<FieldMap>();

  (*field_map)[std::string("numWidgets")] = std::string("int");
  (*field_map)[std::string("diffWidgets")] = std::string("hll");
  sdb.setFields(field_map);

  // test integers
  for (int i=0; i<10; i++) {
    sdb.incr("neil.cook", "numWidgets");
    sdb.incr("neil.cook", "numWidgets");
    int k = sdb.decr("neil.cook", "numWidgets");
    sleep(1);
    // std::cout << k << "\n";
  }
  std::cout << "get_current is " << sdb.get_current("neil.cook", "numWidgets") << "\n";
  std::cout << "get is " << sdb.get("neil.cook", "numWidgets") << "\n";
  std::vector<int> vec;
  sdb.get_windows("neil.cook", "numWidgets", vec);
  for (auto i : vec) {
    std::cout << "window has value " << i << "\n";
  }  

  // test hyperloglog
  for (int i=0; i<10; i++) {
    sdb.add("neil.cook", "diffWidgets", "widget1");
    sdb.add("neil.cook", "diffWidgets", "widget2");
    sdb.add("neil.cook", "diffWidgets", "widget3");
    sleep(1);
    // std::cout << k << "\n";
  }
  std::cout << "get_current is " << sdb.get_current("neil.cook", "diffWidgets") << "\n";
  std::cout << "get is " << sdb.get("neil.cook", "diffWidgets") << "\n";
  vec.clear();
  sdb.get_windows("neil.cook", "diffWidgets", vec);
  for (auto i : vec) {
    std::cout << "window has value " << i << "\n";
  }  
  
}
