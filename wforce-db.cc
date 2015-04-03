#include "wforce.hh"
WForceDB g_wfdb;

void WForceDB::reportTuple(const LoginTuple& lp)
{
  std::lock_guard<std::mutex> lock(d_mutex);
  d_logins.push_back(lp);
}

int defaultAllowTuple(const WForceDB* wfd, const LoginTuple& lp)
{
  if(wfd->countDiffFailures(lp.remote, 1800) > 100)
    return -1;
  if(wfd->countDiffFailures(lp.remote, lp.login, 1800) > 10)
    return -1;
  return 0;
}

std::function<int(const WForceDB*, const LoginTuple&)> g_allow{defaultAllowTuple};

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

