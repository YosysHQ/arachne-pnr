/* Copyright (C) 2015 Cotton Seed
   
   This file is part of arachne-pnr.  Arachne-pnr is free software;
   you can redistribute it and/or modify it under the terms of the GNU
   General Public License version 2 as published by the Free Software
   Foundation.
   
   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>. */

#ifndef PNR_UTIL_HH
#define PNR_UTIL_HH

#include <functional>
#include <iterator>
#include <algorithm>
#include <sstream>
#include <ostream>
#include <string>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <vector>
#include <random>
#include <type_traits>

#include <cassert>

extern const char *version_str;

class random_generator
{
  static const unsigned m = 2147483647;
  static const unsigned a = 48271;
  
  unsigned long long state;
  
public:
  random_generator() : state(1) {}
  random_generator(unsigned seed)
    : state(seed % m)
  {
    assert(seed != 0);
  }
  
  // uniformly random between 0 .. m
  unsigned random()
  {
    state = (a * state) % m;
    return (unsigned)state;
  }
  unsigned operator()()
  {
    return random();
  }
  
  int random_int(int min, int max)
  {
    assert(max >= min);
    unsigned d = max - min + 1;
    assert(d <= m);
    
    unsigned k = m / d;
    assert(k >= 1);
    
    for (;;)
      {
        // randomly distributed 0 .. (m-1)
        unsigned x = random();
        if (x >= k*d)
          continue;
        
        // randomly disributed 0 ..(k*m-1)
        int r = min + (int)(x % d);
        assert(min <= r && r <= max);
        return r;
      }
  }
  
  double random_real(double min, double max)
  {
    assert(max >= min);
    if (min == max)
      return min;
    
    double d = max - min;
    assert(d > 0);
    
    unsigned x = random();
    double r = min + d*(double)x / (double)(m-1);
    assert(min <= r && r <= max);
    return r;
  }
};

extern std::ostream *logs;

template<class T> std::ostream &
operator<<(std::ostream &s, const std::set<T> &S)
{
  s << "{";
  std::copy(S.begin(), S.end(), std::ostream_iterator<T>(s, ", "));
  return s << "}";
}

template<class T> std::ostream &
operator<<(std::ostream &s, const std::unordered_set<T> &S)
{
  s << "{";
  std::copy(S.begin(), S.end(), std::ostream_iterator<T>(s, ", "));
  return s << "}";
}

template<typename K, typename V> struct PrettyKV
{
  const std::pair<K, V> &p;
  PrettyKV(const std::pair<K, V> &p_) : p(p_) {}
};

template<typename K, typename V> inline std::ostream &
operator<<(std::ostream &s, const PrettyKV<K, V> &x)
{
  return s << x.p.first << ": " << x.p.second << "\n";
}

template<typename K, typename V> std::ostream &
operator<<(std::ostream &s, const std::map<K, V> &M)
{
  s << "{";
  std::transform(M.begin(), M.end(),
                 std::ostream_iterator<std::string>(s, ", "),
                 [](const std::pair<K, V> &p) { return PrettyKV<K, V>(p); });
  return s << "}";
}

template<typename K, typename V> std::ostream &
operator<<(std::ostream &s, const std::unordered_map<K, V> &M)
{
  s << "{";
  std::transform(M.begin(), M.end(),
                 std::ostream_iterator<std::string>(s, ", "),
                 [](const std::pair<K, V> &p) { return PrettyKV<K, V>(p); });
  return s << "}";
}

#define fmt(x) (static_cast<const std::ostringstream&>(std::ostringstream() << x).str())

void fatal(const std::string &msg);
void warning(const std::string &msg);
void note(const std::string &msg);

template<typename S, typename T> void
extend(S &s, const T &x)
{
  assert(s.find(x) == s.end());
  s.insert(x);
}

template<typename S, typename T> inline bool
contains(const S &s, const T &x)
{
  return s.find(x) != s.end();
}

template<typename M, typename K, typename V> inline void
extend(M &m, const K &k, const V &v)
{
  assert(m.find(k) == m.end());
  m.insert(std::make_pair(k, v));
}

template<typename M, typename K> inline bool
contains_key(const M &m, const K &k)
{
  return m.find(k) != m.end();
}

template<typename M> inline std::set<typename M::key_type>
keys(const M &m)
{
  std::set<typename M::key_type> keys;
  std::transform(m.begin(), m.end(),
                 std::inserter(keys, keys.end()),
                 [](const typename M::value_type &p) { return p.first; });
  return std::move(keys);
}

std::string unescape(const std::string &s);

template<typename K, typename V> inline const V &
lookup(const std::map<K, V> &M, const K &key)
{
  return M.at(key);
}

template<typename M, typename K, typename V> inline typename M::mapped_type
lookup_or_default(const M &m, const K &key, const V &def)
{
  auto i = m.find(key);
  if (i != m.end())
    return i->second;
  else
    return def;
}

template<typename M, typename K, typename F> inline typename M::mapped_type &
lookup_or_create(M &m, const K &key, const F &f)
{
  auto i = m.find(key);
  if (i == m.end())
    i = m.insert(std::make_pair(key, f())).first;
  return i->second;
}

template<typename C> inline typename C::const_reference
front(const C &c)
{
  assert(c.begin() != c.end());
  return *c.begin();
}

inline bool
is_prefix(const std::string &prefix, const std::string &s)
{
  if (prefix.size() > s.size())
    return false;
  auto r = std::mismatch(prefix.begin(), prefix.end(), 
                         s.begin(),
                         std::equal_to<char>());
  return r.first == prefix.end();
}

inline bool
is_suffix(const std::string &s, const std::string &suffix)
{
  if (suffix.size() > s.size())
    return false;
  auto r = std::mismatch(suffix.rbegin(), suffix.rend(),
                         s.rbegin(),
                         std::equal_to<char>());
  return r.first == suffix.rend();
}

std::string proc_self_dirname();

inline char
hexdigit(int i, char a = 'a')
{
  assert(i >= 0 && i < 16);
  return (i < 10 
          ? '0' + i
          : a + (i - 10));
}

template<typename T> inline const T &
random_element(const std::vector<T> &v, random_generator &rg)
{
  return v[rg.random_int(0, v.size()-1)];
}

inline int
random_int(int min, int max, random_generator &rg)
{
  assert(min <= max);
  return rg.random_int(min, max);
}

template<typename T> inline std::size_t
hash_combine(std::size_t h, const T &v)
{
  std::hash<T> hasher;
  return h ^ (hasher(v) + 0x9e3779b9 + (h << 6) + (h >> 2));
}

namespace std {

template<typename S, typename T>
struct hash<std::pair<S, T>>
{
public:
  size_t operator() (const std::pair<S, T> &p) const
  {
    std::hash<S> Shasher;
    size_t h = Shasher(p.first);
    
    std::hash<T> Thasher;
    return hash_combine(h, Thasher(p.second));
  }
};

}

std::string expand_filename(const std::string &file);

template<typename T> void
pop(std::vector<T> &v, int i)
{
  assert(i < (int)v.size());
  if (i != (int)v.size() - 1)
    std::swap(v[i], v.back());
  v.pop_back();
}

inline
std::string str_to_upper(const std::string &s) {
    std::string res;
    for(auto c : s)
        res += toupper(c);
    return res;
}

inline
bool startswith(const std::string &s, const std::string & pattern) {
  return (s.substr(0, pattern.length()) == pattern);
}

#endif
