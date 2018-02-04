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

#ifndef PNR_BSTREAM_HH
#define PNR_BSTREAM_HH

#include "util.hh"
#include <iostream>
#include <vector>
#include <string>
#include <tuple>
#include <cstring>
#include <cassert>

#include <cstring>
#include <climits>

class obstream
{
private:
  std::ostream &os;
  
public:
  obstream(std::ostream &os_)
    : os(os_)
  {}
  
  void write(const char *p, size_t n)
  {
    os.write(p, n);
    if (os.bad())
      fatal(fmt("std::ostream::write: "
                << strerror(errno)));
  }
};

inline obstream &operator<<(obstream &obs, bool x)
{
  // *logs << "bwrite bool " << x << "\n";
  obs.write(reinterpret_cast<const char *>(&x), sizeof(x));
  return obs;
}

inline obstream &operator<<(obstream &obs, char x)
{
  // *logs << "bwrite char " << x << "\n";
  obs.write(reinterpret_cast<const char *>(&x), sizeof(x));
  return obs;
}

inline obstream &operator<<(obstream &obs, unsigned char x)
{
  // *logs << "bwrite unsigned char " << x << "\n";
  obs.write(reinterpret_cast<const char *>(&x), sizeof(x));
  return obs;
}

template<typename T> obstream &
bwrite_signed_integral_type(obstream &obs, T x)
{
  char buf[sizeof(T) + 1];
  size_t n = 0;
  bool more = true;
  while (more)
    {
      char b = (char)(x & 0x7f);
      x >>= 7;
      
      if ((x == 0
           && !(b & 0x40))
          || (x == -1
              && (b & 0x40) == 0x40))
        more = false;
      else
        b |= 0x80;
      assert(n < sizeof(T) + 1);
      buf[n] = b;
      n ++;
    }
  obs.write(buf, n);
  return obs;
}

template<typename T> obstream &
bwrite_unsigned_integral_type(obstream &obs, T x)
{
  char buf[sizeof(T) + 1];
  size_t n = 0;
  
  bool more = true;
  while (more)
    {
      char b = (char)(x & 0x7f);
      x >>= 7;
      
      if ((x == 0
           && ! (b & 0x40)))
        more = false;
      else
        b |= 0x80;
      
      assert(n < sizeof(T) + 1);
      buf[n] = b;
      n ++;
    }
  obs.write(buf, n);
  return obs;
}  

inline obstream &operator<<(obstream &obs, short x)
{
  return bwrite_signed_integral_type<short>(obs, x);
}

inline obstream &operator<<(obstream &obs, unsigned short x)
{
  return bwrite_unsigned_integral_type<unsigned short>(obs, x);
}

inline obstream &operator<<(obstream &obs, int x)
{
  return bwrite_signed_integral_type<int>(obs, x);
}

inline obstream &operator<<(obstream &obs, unsigned x)
{
  return bwrite_unsigned_integral_type<unsigned>(obs, x);
}

inline obstream &operator<<(obstream &obs, long x)
{
  return bwrite_signed_integral_type<long>(obs, x);
}

inline obstream &operator<<(obstream &obs, unsigned long x)
{
  return bwrite_unsigned_integral_type<unsigned long>(obs, x);
}

inline obstream &operator<<(obstream &obs, unsigned long long x)
{
  return bwrite_unsigned_integral_type<unsigned long long>(obs, x);
}

inline obstream &operator<<(obstream &obs, const std::string &s)
{
  obs << s.size();
  obs.write(&s[0], s.size());
  return obs;
}

template<class T> obstream &
operator<<(obstream &obs, const std::vector<T> &v)
{
  obs << v.size();
  for (const auto &x : v)
    obs << x;
  return obs;
}

template<class T> obstream &
operator<<(obstream &obs, const std::set<T> &s)
{
  obs << s.size();
  for (const auto &x : s)
    obs << x;
  return obs;
}

template<typename K, typename V, typename C> obstream &
operator<<(obstream &obs, const std::map<K, V, C> &m)
{
  obs << m.size();
  for (const auto &x : m)
    obs << x;
  return obs;
}

template<typename F, typename S> inline obstream &
operator<<(obstream &obs, const std::pair<F, S> &p)
{
  return obs << p.first << p.second;
}

template<typename F, typename S, typename T> inline obstream &
operator<<(obstream &obs, const std::tuple<F, S, T> &t)
{
  return obs << std::get<0>(t)
             << std::get<1>(t)
             << std::get<2>(t);
}

class ibstream
{
private:
  std::istream &is;
  
public:
  ibstream(std::istream &is_)
    : is(is_)
  {}
  
  void read(char *p, size_t n)
  {
    is.read(p, n);
    size_t rn = is.gcount();
    if (is.bad()
        || rn != n)
      fatal(fmt("std::istream::read: " << strerror(errno)));
  }
};
 
inline ibstream &operator>>(ibstream &ibs, bool &x)
{
  ibs.read(reinterpret_cast<char *>(&x), sizeof(x));
  // *logs << "bread bool " << x << "\n";
  return ibs;
}

inline ibstream &operator>>(ibstream &ibs, char &x)
{
  ibs.read(reinterpret_cast<char *>(&x), sizeof(x));
  // *logs << "bread char " << x << "\n";
  return ibs;
}

inline ibstream &operator>>(ibstream &ibs, unsigned char &x)
{
  ibs.read(reinterpret_cast<char *>(&x), sizeof(x));
  // *logs << "bread unsigned char " << x << "\n";
  return ibs;
}

template<typename T> ibstream &
bread_signed_integral_type(ibstream &ibs, T &x)
{
  x = 0;
  constexpr int T_bits = CHAR_BIT*sizeof(T);
  int shift = 0;
  for (;;)
    {
      char b;
      ibs >> b;
      x |= (int)(b & 0x7f) << shift;
      shift += 7;
      if (! (b & 0x80))
        {
          if (shift < T_bits
              && (b & 0x40))
            x = (x << (T_bits - shift)) >> (T_bits - shift);
          break;
        }
    }
  return ibs;
}

template<typename T> ibstream &
bread_unsigned_integral_type(ibstream &ibs, T &x)
{
  x = 0;
  unsigned shift = 0;
  for (;;)
    {
      char b;
      ibs >> b;
      x |= ((unsigned)(b & 0x7f) << shift);
      shift += 7;
      if (! (b & 0x80))
        break;
    }
  return ibs;
}

inline ibstream &operator>>(ibstream &ibs, short &x)
{
  return bread_signed_integral_type<short>(ibs, x);
}

inline ibstream &operator>>(ibstream &ibs, unsigned short &x)
{
  return bread_unsigned_integral_type<unsigned short>(ibs, x);
}

inline ibstream &operator>>(ibstream &ibs, int &x)
{
  return bread_signed_integral_type<int>(ibs, x);
}

inline ibstream &operator>>(ibstream &ibs, unsigned &x)
{
  return bread_unsigned_integral_type<unsigned>(ibs, x);
}

inline ibstream &operator>>(ibstream &ibs, long &x)
{
  return bread_signed_integral_type<long>(ibs, x);
}

inline ibstream &operator>>(ibstream &ibs, unsigned long &x)
{
  return bread_unsigned_integral_type<unsigned long>(ibs, x);
}

inline ibstream &operator>>(ibstream &ibs, unsigned long long &x)
{
  return bread_unsigned_integral_type<unsigned long long>(ibs, x);
}

inline ibstream &operator>>(ibstream &ibs, std::string &s)
{
  size_t n;
  ibs >> n;
  s.resize(n);
  ibs.read(&s[0], n);
  s[n] = 0;
  return ibs;
}

template<class T> ibstream &
operator>>(ibstream &ibs, std::vector<T> &v)
{
  size_t n;
  ibs >> n;
  v.resize(n);
  for (size_t i = 0; i < n; ++i)
    ibs >> v[i];
  return ibs;
}

template<class T> ibstream &
operator>>(ibstream &ibs, std::set<T> &s)
{
  s.clear();
  size_t n;
  ibs >> n;
  for (size_t i = 0; i < n; ++i)
    {
      T x;
      ibs >> x;
      s.insert(x);
    }
  return ibs;
}

template<typename K, typename V, typename C> ibstream &
operator>>(ibstream &ibs, std::map<K, V, C> &m)
{
  m.clear();
  size_t n;
  ibs >> n;
  for (size_t i = 0; i < n; ++i)
    {
      std::pair<K, V> x;
      ibs >> x;
      m.insert(x);
    }
  return ibs;
}


template<typename F, typename S> inline ibstream &
operator>>(ibstream &ibs, std::pair<F, S> &p)
{
  return ibs >> p.first >> p.second;
}

template<typename F, typename S, typename T> inline ibstream &
operator>>(ibstream &ibs, std::tuple<F, S, T> &t)
{
  return ibs >> std::get<0>(t)
             >> std::get<1>(t)
             >> std::get<2>(t);
}


#endif
