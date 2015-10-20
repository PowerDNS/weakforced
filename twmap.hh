#include <string>
#include <vector>
#include <memory>
#include <map>
#include <ctime>

class TWStatsMember;

typedef std::shared_ptr<TWStatsMember> TWStatsMemberP;

// Not all stats member subclasses will implement all of these methods in a useful way (e.g. incr is meaningless to a hll)
class TWStatsMember
{
public:
  virtual int incr() = 0; // add 1 to the stored value
  virtual int decr() = 0; // subtract 1 from the stored value
  virtual int add(int a) = 0; // add an integer to the stored value
  virtual int add(const std::string& s) = 0; // add a string to the stored window (this has different semantics for each subclass)
  virtual int sub(int a) = 0; // subtract an integer from the stored value
  virtual int sub(const std::string& s) = 0; // remove a string from the stored value
  virtual int get() = 0; // return the current stat
  virtual void set(int) = 0; // Set the stored value to the supplied integer
  virtual void set(const std::string& s) = 0; // Set the stored value based on the supplied string
  virtual int sum(const std::vector<TWStatsMemberP>& vec) = 0; // combine an array of stored values
};

class TWStatsMemberInt;

typedef std::shared_ptr<TWStatsMemberInt> TWStatsMemberIntP;

class TWStatsMemberInt : public TWStatsMember
{
public:
  int incr() { i++; return i; }
  int decr() { i--; return i; }
  int add(int a) { i += a; return i; }
  int add(const std::string& s) { return i; }
  int sub(int a) { i -= a; return i; }
  int sub(const std::string& s) { return i; }
  int get() { return i; }
  void set(int a) { i = a; }
  void set(const std::string& s) { return; }
  int sum(const std::vector<TWStatsMemberP>& vec)
  {
    int count = 0;
    for (auto a : vec)
      {
        count += a->get();
      }
    return count;
  }
private:
  int i=0;
};

template<typename T> TWStatsMemberP createInstance() { return std::make_shared<T>(); }

typedef std::map<std::string, TWStatsMemberP(*)()> map_type;

// This needs to be instanstiated
struct TWStatsTypeMap
{
  map_type type_map;
  TWStatsTypeMap()
  {
    type_map["int"] = &createInstance<TWStatsMemberInt>;
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
    window_size = w_siz;
    num_windows = num_w;
    std::time(&start_time);
  }
  bool setFields(std::shared_ptr<FieldMap> fieldsp);
  int incr(const T& key, const std::string& field_name);
  int decr(const T& key, const std::string& field_name);
  int add(const T& key, const std::string& field_name, int a);
  int add(const T& key, const std::string& field_name, const std::string& s);
  int sub(const T& key, const std::string& field_name, int a);
  int sub(const T& key, const std::string& field_name, const std::string& s);
  int get(const T& key, const std::string& field_name); // gets all fields summed/combined over all windows
  int get_current(const T& key, const std::string& field_name); // gets the value just for the current window
  std::vector<int> get_windows(const T& key, const std::string& field_name); // gets each window value returned in a vector
  void set(const T& key, const std::string& field_name, int a);
  void set(const T& key, const std::string& field_name, const std::string& s);
protected:
  bool find_create_key_field(const T& key, const std::string& field_name, std::vector<TWStatsMemberP>& ret_vecfg
);
private:
  typedef std::map<T, std::map<std::string, std::vector<TWStatsMemberP>>> TWStatsDBMap;
  TWStatsDBMap stats_db;
  std::shared_ptr<FieldMap> field_map;
  int window_size;
  int num_windows;
  std::time_t start_time;
};
