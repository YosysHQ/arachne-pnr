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

#include <vector>

#include <cstddef>
#include <cstdint>
#include <cassert>

template<size_t B>
class BasedBitVector
{
  int n;
  std::vector<uint64_t> v;
  
public:
  class BitRef
  {
    std::vector<uint64_t> &v;
    size_t i;
    
  public:
    BitRef (std::vector<uint64_t> &v_, size_t i_)
      : v(v_), i(i_)
    {}
    
    BitRef &operator=(bool x)
    {
      size_t w = (i - B) / 64,
        b = (i - B) & 63;
      uint64_t m = ((uint64_t)1 << b);
      assert(w < v.size());
      if (x)
        v[w] |= m;
      else
        v[w] &= ~m;
      return *this;
    }
    
    operator bool() const
    {
      size_t w = (i - B) / 64,
        b = (i - B) & 63;
      uint64_t m = ((uint64_t)1 << b);
      assert(w < v.size());
      return v[w] & m;
    }
  };
  
  BasedBitVector() {}
  BasedBitVector(size_t n_)
    : n(n_), v((n + 63) / 64, 0)
  {
  }
  
  BasedBitVector(size_t n_, uint64_t init)
    : n(n_), v((n + 63) / 64, 0)
  {
    v[0] = init;
  }
  
  void resize(size_t n_)
  {
    n = n_;
    v.resize((n + 63) / 64, 0);
  }
  
  void zero()
  {
    std::fill(v.begin(), v.end(), 0);
  }
  size_t size() const { return n; }
  
  bool operator[](size_t i) const
  {
    assert(i >= B && i < n + B);
    size_t w = (i - B) / 64,
      b = (i - B) & 63;
    uint64_t m = ((uint64_t)1 << b);
    return v[w] & m;
  }
  BitRef operator[](size_t i)
  {
    assert(i >= B && i < n + B);
    return BitRef(v, i);
  }
};

using BitVector = BasedBitVector<0>;
using BitVector1 = BasedBitVector<1>;

#endif
