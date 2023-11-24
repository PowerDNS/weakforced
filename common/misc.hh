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

#ifndef MISC_HH
#define MISC_HH
#include <inttypes.h>
#include <cstring>
#include <cstdio>
#include <regex.h>
#include <limits.h>
#include <boost/algorithm/string.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/sequenced_index.hpp>
using namespace ::boost::multi_index;

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <syslog.h>
#include <deque>
#include <stdexcept>
#include <string>
#include <ctype.h>
#include <vector>
#include "namespaces.hh"
string humanDuration(time_t passed);
void stripLine(string &line);
string getHostname();
string urlEncode(const string &text);
int waitForData(int fd, int seconds, int useconds=0);
int waitFor2Data(int fd1, int fd2, int seconds, int useconds, int* fd);
int waitForRWData(int fd, bool waitForRead, int seconds, int useconds, bool* error=nullptr, bool* disconnected=nullptr);
uint16_t getShort(const unsigned char *p);
uint16_t getShort(const char *p);
uint32_t getLong(const unsigned char *p);
uint32_t getLong(const char *p);
uint32_t pdns_strtoui(const char *nptr, char **endptr, int base);


struct ServiceTuple
{
  string host;
  uint16_t port;
};
void parseService(const string &descr, ServiceTuple &st);
string humanTime(time_t t);
template <typename Container>
void
stringtok (Container &container, string const &in,
           const char * const delimiters = " \t\n")
{
  const string::size_type len = in.length();
  string::size_type i = 0;
  
  while (i<len) {
    // eat leading whitespace
    i = in.find_first_not_of (delimiters, i);
    if (i == string::npos)
      return;   // nothing left but white space
    
    // find the end of the token
    string::size_type j = in.find_first_of (delimiters, i);
    
    // push token
    if (j == string::npos) {
      container.push_back (in.substr(i));
      return;
    } else
      container.push_back (in.substr(i, j-i));
    
    // set up for next loop
    i = j + 1;
  }
}

template<typename T> bool rfc1982LessThan(T a, T b)
{
  return ((signed)(a - b)) < 0;
}

// fills container with ranges, so {posbegin,posend}
template <typename Container>
void
vstringtok (Container &container, string const &in,
           const char * const delimiters = " \t\n")
{
  const string::size_type len = in.length();
  string::size_type i = 0;
  
  while (i<len) {
    // eat leading whitespace
    i = in.find_first_not_of (delimiters, i);
    if (i == string::npos)
      return;   // nothing left but white space
    
    // find the end of the token
    string::size_type j = in.find_first_of (delimiters, i);
    
    // push token
    if (j == string::npos) {
      container.push_back (make_pair(i, len));
      return;
    } else
      container.push_back (make_pair(i, j));
    
    // set up for next loop
    i = j + 1;
  }
}

int writen2(int fd, const void *buf, size_t count);
inline int writen2(int fd, const std::string &s) { return writen2(fd, s.data(), s.size()); }
int readn2(int fd, void* buffer, unsigned int len);

const string toLower(const string &upper);
const string toLowerCanonic(const string &upper);
bool IpToU32(const string &str, uint32_t *ip);
string U32ToIP(uint32_t);
string stringerror();
string netstringerror();
string itoa(int i);
string uitoa(unsigned int i);
string bitFlip(const string &str);

void cleanSlashes(string &str);

/** The DTime class can be used for timing statistics with microsecond resolution. 
On 32 bits systems this means that 2147 seconds is the longest time that can be measured. */
class DTime 
{
public:
  DTime(); //!< Does not set the timer for you! Saves lots of gettimeofday() calls
  DTime(const DTime &dt);
  time_t time();
  inline void set();  //!< Reset the timer
  inline int udiff(); //!< Return the number of microseconds since the timer was last set.
  inline int udiffNoReset(); //!< Return the number of microseconds since the timer was last set.
  void setTimeval(const struct timeval& tv)
  {
    d_set=tv;
  }
  struct timeval getTimeval()
  {
    return d_set;
  }
private:
  struct timeval d_set;
};

inline void DTime::set()
{
  gettimeofday(&d_set,0);
}

inline int DTime::udiff()
{
  int res=udiffNoReset();
  gettimeofday(&d_set,0);
  return res;
}

inline int DTime::udiffNoReset()
{
  struct timeval now;

  gettimeofday(&now,0);
  int ret=1000000*(now.tv_sec-d_set.tv_sec)+(now.tv_usec-d_set.tv_usec);
  return ret;
}


inline bool dns_isspace(char c)
{
  return c==' ' || c=='\t' || c=='\r' || c=='\n';
}

inline char dns_tolower(char c)
{
  if(c>='A' && c<='Z')
    c+='a'-'A';
  return c;
}

inline const string toLower(const string &upper)
{
  string reply(upper);
  for(unsigned int i = 0; i < reply.length(); i++) {
    char c = dns_tolower(upper[i]);
    if (c != upper[i])
      reply[i] = c;
  }
  return reply;
}

inline const string toLowerCanonic(const string &upper)
{
  string reply(upper);
  if(!upper.empty()) {
    unsigned int i, limit= ( unsigned int ) reply.length();
    for(i = 0; i < limit ; i++) {
      char c = dns_tolower(upper[i]);
      if (c != upper[i])
        reply[i] = c;
    }   
    if(upper[i-1]=='.')
      reply.resize(i-1);
  }
      
  return reply;
}



// Make s uppercase:
inline string toUpper( const string& s )
{
        string r(s);
        for( unsigned int i = 0; i < s.length(); i++ ) {
        	r[i] = toupper( r[i] );
        }
        return r;
}

inline double getTime()
{
  struct timeval now;
  gettimeofday(&now,0);
  
  return now.tv_sec+now.tv_usec/1000000.0;
}

inline void unixDie(const string &why)
{
  throw runtime_error(why+": "+strerror(errno));
}

string makeHexDump(const string& str);

void normalizeTV(struct timeval& tv);
const struct timeval operator+(const struct timeval& lhs, const struct timeval& rhs);
const struct timeval operator-(const struct timeval& lhs, const struct timeval& rhs);
inline float makeFloat(const struct timeval& tv)
{
  return tv.tv_sec + tv.tv_usec/1000000.0f;
}

inline bool operator<(const struct timeval& lhs, const struct timeval& rhs) 
{
  return make_pair(lhs.tv_sec, lhs.tv_usec) < make_pair(rhs.tv_sec, rhs.tv_usec);
}

inline bool pdns_ilexicographical_compare(const std::string& a, const std::string& b)  __attribute__((pure));
inline bool pdns_ilexicographical_compare(const std::string& a, const std::string& b)
{
  const unsigned char *aPtr = (const unsigned char*)a.c_str(), *bPtr = (const unsigned char*)b.c_str();
  const unsigned char *aEptr = aPtr + a.length(), *bEptr = bPtr + b.length();
  while(aPtr != aEptr && bPtr != bEptr) {
    if ((*aPtr != *bPtr) && (dns_tolower(*aPtr) - dns_tolower(*bPtr)))
      return (dns_tolower(*aPtr) - dns_tolower(*bPtr)) < 0;
    aPtr++;
    bPtr++;
  }
  if(aPtr == aEptr && bPtr == bEptr) // strings are equal (in length)
    return false;
  return aPtr == aEptr; // true if first string was shorter
}

inline bool pdns_iequals(const std::string& a, const std::string& b) __attribute__((pure));
inline bool pdns_iequals(const std::string& a, const std::string& b)
{
  if (a.length() != b.length())
    return false;

  const char *aPtr = a.c_str(), *bPtr = b.c_str();
  const char *aEptr = aPtr + a.length();
  while(aPtr != aEptr) {
    if((*aPtr != *bPtr) && (dns_tolower(*aPtr) != dns_tolower(*bPtr)))
      return false;
    aPtr++;
    bPtr++;
  }
  return true;
}

inline bool pdns_iequals_ch(const char a, const char b) __attribute__((pure));
inline bool pdns_iequals_ch(const char a, const char b)
{
  if ((a != b) && (dns_tolower(a) != dns_tolower(b)))
    return false;

  return true;
}

// lifted from boost, with thanks
class AtomicCounter
{
public:
    typedef unsigned long native_t;
    explicit AtomicCounter( native_t v = 0) : value_( v ) {}

    native_t operator++()
    {
      return atomic_exchange_and_add( &value_, +1 ) + 1;
    }

    native_t operator++(int)
    {
      return atomic_exchange_and_add( &value_, +1 );
    }

    native_t operator+=(native_t val)
    {
      return atomic_exchange_and_add( &value_, val );
    }

    native_t operator-=(native_t val)
    {
      return atomic_exchange_and_add( &value_, -val );
    }

    native_t operator--()
    {
      return atomic_exchange_and_add( &value_, -1 ) - 1;
    }

    operator native_t() const
    {
      return atomic_exchange_and_add( &value_, 0);
    }

    AtomicCounter(AtomicCounter const &rhs) : value_(rhs)
    {
    }

private:
    mutable native_t value_;
    
    // the below is necessary because __sync_fetch_and_add is not universally available on i386.. I 3> RHEL5. 
#if defined( __GNUC__ ) && ( defined( __i386__ ) || defined( __x86_64__ ) )
    static native_t atomic_exchange_and_add( native_t * pw, native_t dv )
    {
        // int r = *pw;
        // *pw += dv;
        // return r;

        native_t r;

        __asm__ __volatile__
        (
            "lock\n\t"
            "xadd %1, %0":
            "+m"( *pw ), "=r"( r ): // outputs (%0, %1)
            "1"( dv ): // inputs (%2 == %1)
            "memory", "cc" // clobbers
        );

        return r;
    }
    #else 
    static native_t atomic_exchange_and_add( native_t * pw, native_t dv )
    {
      return __sync_fetch_and_add(pw, dv);
    }
    #endif
};

// FIXME400 this should probably go?
struct CIStringCompare
{
  bool operator()(const string& a, const string& b) const
  {
    return pdns_ilexicographical_compare(a, b);
  }
};

struct CIStringComparePOSIX
{
  bool operator() (const std::string& lhs, const std::string& rhs) const
  {
    const std::locale &loc = std::locale("POSIX");
    auto lhsIter = lhs.begin();
    auto rhsIter = rhs.begin();
    while (lhsIter != lhs.end()) {
      if (rhsIter == rhs.end() || std::tolower(*rhsIter,loc) < std::tolower(*lhsIter,loc)) {
        return false;
      }
      if (std::tolower(*lhsIter,loc) < std::tolower(*rhsIter,loc)) {
        return true;
      }
      ++lhsIter;++rhsIter;
    }
    return rhsIter != rhs.end();
  }
};

struct CIStringPairCompare
{
  bool operator()(const pair<string, uint16_t>& a, const pair<string, uint16_t>& b) const
  {
    if(pdns_ilexicographical_compare(a.first, b.first))
      return true;
    if(pdns_ilexicographical_compare(b.first, a.first))
      return false;
    return a.second < b.second;
  }
};

inline size_t pdns_ci_find(const string& haystack, const string& needle)
{
  string::const_iterator it = std::search(haystack.begin(), haystack.end(),
    needle.begin(), needle.end(), pdns_iequals_ch);
  if (it == haystack.end()) {
    // not found
    return string::npos;
  } else {
    return it - haystack.begin();
  }
}

pair<string, string> splitField(const string& inp, char sepa);

inline bool isCanonical(const string& dom)
{
  if(dom.empty())
    return false;
  return dom[dom.size()-1]=='.';
}

inline string toCanonic(const string& zone, const string& domain)
{
  if(domain.length()==1 && domain[0]=='@')
    return zone;

  if(isCanonical(domain))
    return domain;
  string ret=domain;
  ret.append(1,'.');
  if(!zone.empty() && zone[0]!='.')
    ret.append(zone);
  return ret;
}

string stripDot(const string& dom);
void seedRandom(const string& source);
string makeRelative(const std::string& fqdn, const std::string& zone);
string labelReverse(const std::string& qname);
std::string dotConcat(const std::string& a, const std::string &b);
int makeIPv6sockaddr(const std::string& addr, struct sockaddr_in6* ret);
int makeIPv4sockaddr(const std::string& str, struct sockaddr_in* ret);
int makeUNsockaddr(const std::string& path, struct sockaddr_un* ret);
bool stringfgets(FILE* fp, std::string& line);

template<typename Index>
std::pair<typename Index::iterator,bool>
replacing_insert(Index& i,const typename Index::value_type& x)
{
  std::pair<typename Index::iterator,bool> res=i.insert(x);
  if(!res.second)res.second=i.replace(res.first,x);
  return res;
}

/** very small regex wrapper */
class Regex
{
public:
  /** constructor that accepts the expression to regex */
  Regex(const string &expr);
  
  ~Regex()
  {
    regfree(&d_preg);
  }
  /** call this to find out if 'line' matches your expression */
  bool match(const string &line)
  {
    return regexec(&d_preg,line.c_str(),0,0,0)==0;
  }
  
private:
  regex_t d_preg;
};

union ComboAddress;
/* itfIndex is an interface index, as returned by if_nametoindex(). 0 means default. */
void addCMsgSrcAddr(struct msghdr* msgh, void* cmsgbuf, const ComboAddress* source, int itfIndex);

unsigned int getFilenumLimit(bool hardOrSoft=0);
void setFilenumLimit(unsigned int lim);
bool readFileIfThere(const char* fname, std::string* line);
uint32_t burtle(const unsigned char* k, uint32_t length, uint32_t init);
void setSocketTimestamps(int fd);

//! Sets the socket into blocking mode.
bool setBlocking( int sock );

//! Sets the socket into non-blocking mode.
bool setNonBlocking( int sock );
int closesocket(int fd);
bool setCloseOnExec(int sock);

unsigned int pdns_stou(const std::string& str, size_t * idx = 0, int base = 10);

std::string getDirectoryPath(const std::string& filename);
std::string getFileFromPath(const std::string& filename);

typedef std::vector<std::pair<std::string, std::string>> KeyValVector;

#endif

