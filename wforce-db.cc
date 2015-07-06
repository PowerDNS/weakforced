#include "wforce.hh"
WForceDB g_wfdb;

/* for higher capacity, ponder shifting to __gnu_cxx::__sso_string for storage
   Assume we'll have to store 10 million tuples. 
*/

size_t WForceDB::size()
{
  std::lock_guard<std::mutex> lock(d_mutex);
  return d_logins.size();
}

void WForceDB::clear()
{
  std::lock_guard<std::mutex> lock(d_mutex);
  d_logins.clear();
}

int WForceDB::clearLogin(const std::string& login) 
{
  std::lock_guard<std::mutex> lock(d_mutex);
  
  auto& loginindex = boost::multi_index::get<LoginTag>(d_logins);
  auto count = loginindex.count(login);
  auto range = loginindex.equal_range(login);
  loginindex.erase(range.first, range.second);
  return count;
}

int WForceDB::clearRemote(const ComboAddress& remote) 
{
  std::lock_guard<std::mutex> lock(d_mutex);
  
  auto& remoteindex = boost::multi_index::get<RemoteTag>(d_logins);
  auto count = remoteindex.count(remote);
  auto range = remoteindex.equal_range(remote);
  remoteindex.erase(range.first, range.second);
  return count;
}


void WForceDB::reportTuple(const LoginTuple& lp)
{
  std::lock_guard<std::mutex> lock(d_mutex);
  d_logins.insert(lp);

  //  if(!res.second)
  //  cerr<<"Dup!"<<endl; // XXX do some accounting with this
}

void WForceDB::timePurge(int seconds)
{
  std::lock_guard<std::mutex> lock(d_mutex);
  double now=time(0);
  double limit=now-seconds;

  auto& time_index = boost::multi_index::get<TimeTag>(d_logins);
  time_index.erase(time_index.begin(), time_index.lower_bound(limit));

}

void WForceDB::numberPurge(int amount)
{
  std::lock_guard<std::mutex> lock(d_mutex);
  auto toRemove = d_logins.size() - amount;
  if(toRemove <= 0) {
    d_logins.clear();
  }
  else {
    auto& seq_index = boost::multi_index::get<SeqTag>(d_logins);
    auto end = seq_index.begin();
    std::advance(end, toRemove);
    seq_index.erase(seq_index.begin(), end);
  }
}


int WForceDB::countFailures(const ComboAddress& remote, int seconds) const
{

  int count=0;
  time_t now=time(0);
  std::lock_guard<std::mutex> lock(d_mutex);

  auto& remindex = boost::multi_index::get<RemoteTag>(d_logins);
  auto range = remindex.equal_range(remote);
  for(auto iter = range.first; iter != range.second ; ++iter) {
    if((iter->t > now - seconds) && !iter->success)
      count++;
  }
  return count;
}

vector<LoginTuple> WForceDB::getTuples() const
{   
  std::lock_guard<std::mutex> lock(d_mutex);
  vector<LoginTuple> ret;
  ret.reserve(d_logins.size());

  auto& seq_index = boost::multi_index::get<SeqTag>(d_logins);
  for(const auto& a : seq_index)
    ret.push_back(a);
  return ret;
}

vector<LoginTuple> WForceDB::getTuplesLogin(const std::string& login) const
{   
  std::lock_guard<std::mutex> lock(d_mutex);
  vector<LoginTuple> ret;

  auto& loginindex = boost::multi_index::get<LoginTag>(d_logins);
  auto range = loginindex.equal_range(login);
  for(auto iter = range.first; iter != range.second ; ++ iter)
    ret.push_back(*iter);
  return ret;
}

vector<LoginTuple> WForceDB::getTuplesRemote(const ComboAddress& remote) const
{   
  std::lock_guard<std::mutex> lock(d_mutex);
  vector<LoginTuple> ret;

  auto& remoteindex = boost::multi_index::get<RemoteTag>(d_logins);
  auto range = remoteindex.equal_range(remote);
  for(auto iter = range.first; iter != range.second ; ++ iter)
    ret.push_back(*iter);
  return ret;
}


int WForceDB::countDiffFailures(const ComboAddress& remote, int seconds) const
{
  time_t now=time(0);
  set<pair<string,string>> attempts;
  std::lock_guard<std::mutex> lock(d_mutex);
  auto& remindex = boost::multi_index::get<RemoteTag>(d_logins);
  auto range = remindex.equal_range(remote);
  for(auto iter = range.first; iter != range.second ; ++iter) {
    if((iter->t > now - seconds) && !iter->success)
      attempts.insert({iter->login, iter->pwhash});
  }
  return attempts.size();
}

int WForceDB::countDiffFailures(const ComboAddress& remote, string login, int seconds) const
{
  time_t now=time(0);
  set<pair<string,string>> attempts;
  std::lock_guard<std::mutex> lock(d_mutex);

  auto& remindex = boost::multi_index::get<RemoteTag>(d_logins);
  auto range = remindex.equal_range(remote);
  for(auto iter = range.first; iter != range.second ; ++iter) {
    if((iter->t > now - seconds) && !iter->success && iter->login==login)
      attempts.insert({iter->login, iter->pwhash});
  }
  return attempts.size();
}

