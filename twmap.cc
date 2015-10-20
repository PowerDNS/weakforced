#include "twmap.hh"
#include <iostream>

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
bool TWStatsDB<T>::find_create_key_field(const T& key, const std::string& field_name, std::vector<TWStatsMemberP>& ret_vec)
{
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
      auto mystats = myfm->second;
      ret_vec = mystats;
      return true;
    }
    else {
      // if we get here it means the key exists, but not the field so we need to add it
      auto mystat = mytype->second;
      for (int i=0; i< num_windows; i++) {
	ret_vec.push_back(mystat());
      }
      mysdb->second.insert(std::pair<std::string, std::vector<TWStatsMemberP>>(field_name, ret_vec));
    }
  }
  else {
    // if we get here, we have no key and no field
    auto mystat = mytype->second;
    for (int i=0; i< num_windows; i++) {
      ret_vec.push_back(mystat());
    }
    std::map<std::string, std::vector<TWStatsMemberP>> myfm;
    myfm.insert(std::pair<std::string, std::vector<TWStatsMemberP>>(field_name, ret_vec));
    stats_db.insert(std::pair<T, std::map<std::string, std::vector<TWStatsMemberP>>>(key, myfm));
  }
  return true;
}

template <typename T>
int TWStatsDB<T>::incr(const T& key, const std::string& field_name)
{
  std::vector<TWStatsMemberP> myvec;

  if (find_create_key_field(key, field_name, myvec) != true) {
    return 0;
  }
}


main (int argc, char** argv)
{
  auto ap=createInstance<TWStatsMemberInt>();
  auto bp=createInstance<TWStatsMemberInt>();
  std::vector<TWStatsMemberP> vec;

  ap->add(10);
  bp->add(10);
  vec.push_back(ap);
  vec.push_back(bp);

  // std::cout << ap->sum(vec) << "\n";

  TWStatsDB<std::string> sdb(600,6);
  auto field_map = std::make_shared<FieldMap>();
  (*field_map)[std::string("numWidgets")] = std::string("int");
  sdb.setFields(field_map);
  sdb.incr("neil.cook", "numWidgets");
}
