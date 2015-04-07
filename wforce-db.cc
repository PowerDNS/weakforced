#include "wforce.hh"
WForceDB g_wfdb;

/* for higher capacity, ponder shifting to __gnu_cxx::__sso_string for storage
   Assume we'll have to store 10 million tuples. 
*/

void WForceDB::reportTuple(const LoginTuple& lp)
{
  std::lock_guard<std::mutex> lock(d_mutex);
  d_logins.push_back(lp);
}

void WForceDB::timePurge(int seconds)
{
  std::lock_guard<std::mutex> lock(d_mutex);
  time_t limit=time(0)-seconds;

  d_logins.erase(remove_if(d_logins.begin(), d_logins.end(), [limit](const LoginTuple& lt) { return lt.t < limit; }), d_logins.end());
}

void WForceDB::numberPurge(int amount)
{
  std::lock_guard<std::mutex> lock(d_mutex);
  auto toRemove = d_logins.size() - amount;
  if(toRemove <= 0)
    d_logins.clear();
  else
    d_logins.erase(d_logins.begin() + toRemove, d_logins.end());
}


int WForceDB::countFailures(const ComboAddress& remote, int seconds) const
{

  int count=0;
  time_t now=time(0);
  std::lock_guard<std::mutex> lock(d_mutex);
  for(const auto& lt : d_logins) {
    if((lt.t > now - seconds) && !lt.success && ComboAddress::addressOnlyEqual()(lt.remote,remote))
      count++;
  }
  return count;
}

vector<LoginTuple> WForceDB::getTuples() const
{   
  std::lock_guard<std::mutex> lock(d_mutex); 
  return d_logins; 
}

int WForceDB::countDiffFailures(const ComboAddress& remote, int seconds) const
{
  time_t now=time(0);
  set<pair<string,string>> attempts;
  std::lock_guard<std::mutex> lock(d_mutex);
  for(const auto& lt : d_logins) {
    if((lt.t > now - seconds) && !lt.success && ComboAddress::addressOnlyEqual()(lt.remote,remote))
      attempts.insert({lt.login, lt.pwhash});
  }
  return attempts.size();
}

int WForceDB::countDiffFailures(const ComboAddress& remote, string login, int seconds) const
{
  time_t now=time(0);
  set<pair<string,string>> attempts;
  std::lock_guard<std::mutex> lock(d_mutex);
  for(const auto& lt : d_logins) {
    if((lt.t > now - seconds) && !lt.success && lt.login==login && ComboAddress::addressOnlyEqual()(lt.remote,remote))
      attempts.insert({lt.login, lt.pwhash});
  }
  return attempts.size();
}

