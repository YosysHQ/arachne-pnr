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

#ifndef PNR_BITVECTOR_HH
#define PNR_BITVECTOR_HH

#include <cstdint>
#include <vector>
#include <cassert>

class BitVector
{
  int n;
  std::vector<uint64_t> v;
  
public:
  class BitRef
  {
    std::vector<uint64_t> &v;
    int i;
    
  public:
    BitRef (std::vector<uint64_t> &v_, int i_)
      : v(v_), i(i_)
    {}
    
    BitRef &operator=(bool x)
    {
      int w = i / 64,
	b = i & 63;
      uint64_t m = ((uint64_t)1 << b);
      assert(w < (int)v.size());
      if (x)
	v[w] |= m;
      else
	v[w] &= ~m;
      return *this;
    }
    
    operator bool() const
    {
      int w = i / 64,
	b = i & 63;
      uint64_t m = ((uint64_t)1 << b);
      assert(w < (int)v.size());
      return v[w] & m;
    }
  };
  
  BitVector() {}
  BitVector(int n_)
    : n(n_), v((n + 63) / 64, 0)
  {
  }
  
  BitVector(int n_, uint64_t init)
    : n(n_), v((n + 63) / 64, 0)
  {
    v[0] = init;
  }
  
  void resize(int n_)
  {
    n = n_;
    v.resize((n + 63) / 64, 0);
  }
  
  void zero()
  {
    std::fill(v.begin(), v.end(), 0);
  }
  int size() const { return n; }
  
  bool operator[](int i) const
  {
    assert(i < n);
    int w = i / 64,
      b = i & 63;
    uint64_t m = ((uint64_t)1 << b);
    return v[w] & m;
  }
  BitRef operator[](int i)
  {
    assert(i < n);
    return BitRef(v, i);
  }
};

#endif
