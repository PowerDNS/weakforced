#include <string>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <ctime>
#include <mutex>
#include <thread>
#include "hyperloglog.hpp"

class TWStatsMember;

typedef std::shared_ptr<TWStatsMember> TWStatsMemberP;
typedef std::vector<std::pair<std::time_t, TWStatsMemberP>> TWStatsBuf;

// Not all stats member subclasses will implement all of these methods in a useful way (e.g. incr is meaningless to a hll)
class TWStatsMember
{
public:
  virtual void add(int a) = 0; // add an integer to the stored value
  virtual void add(const std::string& s) = 0; // add a string to the stored window (this has different semantics for each subclass)
  virtual void sub(int a) = 0; // subtract an integer from the stored value
  virtual void sub(const std::string& s) = 0; // remove a string from the stored value
  virtual int get() = 0; // return the current stat
  virtual void set(int) = 0; // Set the stored value to the supplied integer
  virtual void set(const std::string& s) = 0; // Set the stored value based on the supplied string
  virtual void erase() = 0; // Remove all data - notice this isn't necessarily the same as set(0) or set("")
  virtual int sum(const TWStatsBuf& vec) = 0; // combine an array of stored values
};

class TWStatsMemberInt;

typedef std::shared_ptr<TWStatsMemberInt> TWStatsMemberIntP;

class TWStatsMemberInt : public TWStatsMember
{
public:
  void add(int a) { i += a; }
  void add(const std::string& s) { return; }
  void sub(int a) { i -= a;  }
  void sub(const std::string& s) { return; }
  int get() { return i; }
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
  void sub(int a) { return; }
  void sub(const std::string& s) { return; }
  int get() { return hllp->estimate(); }
  void set(int a) { return; }
  void set(const std::string& s) { hllp->clear(); hllp->add(s.c_str(), s.length()); }
  void erase() { hllp->clear(); }
  int sum(const TWStatsBuf& vec)
  {
    int count = 0;
    hll::HyperLogLog hllsum(HLL_NUM_REGISTER_BITS);
    for (auto a : vec)
      {
	// XXX yes not massively pretty
	std::shared_ptr<TWStatsMemberHLL> twp = std::dynamic_pointer_cast<TWStatsMemberHLL>(a.second);
	hllsum.merge((*(twp->hllp)));
      }
    return hllsum.estimate();
  }
private:
  std::shared_ptr<hll::HyperLogLog> hllp;
};


template<typename T> TWStatsMemberP createInstance() { return std::make_shared<T>(); }

typedef std::map<std::string, TWStatsMemberP(*)()> map_type;

// This is instantiated in twmap.cc
struct TWStatsTypeMap
{
  map_type type_map;
  TWStatsTypeMap()
  {
    type_map["int"] = &createInstance<TWStatsMemberInt>;
    type_map["hll"] = &createInstance<TWStatsMemberHLL>;
  }
};

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
  bool setFields(std::shared_ptr<FieldMap> fieldsp);
  int incr(const T& key, const std::string& field_name);
  int decr(const T& key, const std::string& field_name);
  int add(const T& key, const std::string& field_name, int a); // returns all fields summed/combined
  int add(const T& key, const std::string& field_name, const std::string& s); // returns all fields summed/combined
  int sub(const T& key, const std::string& field_name, int a); // returns all fields summed/combined
  int sub(const T& key, const std::string& field_name, const std::string& s); // returns all fields summed/combined
  int get(const T& key, const std::string& field_name); // gets all fields summed/combined over all windows
  int get_current(const T& key, const std::string& field_name); // gets the value just for the current window
  bool get_windows(const T& key, const std::string& field_name, std::vector<int>& ret_vec); // gets each window value returned in a vector
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
  std::shared_ptr<FieldMap> field_map;
  int window_size;
  int num_windows;
  int cur_window = 0;
  std::time_t start_time;
  mutable std::mutex mutx;
};
