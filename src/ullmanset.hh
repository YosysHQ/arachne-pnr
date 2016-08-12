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

#ifndef PNR_ULLMANSET_HH
#define PNR_ULLMANSET_HH

#include "vector.hh"

#include <cstddef>
#include <cassert>

template<unsigned B>
class BasedUllmanSet
{
  size_t n;
  std::vector<int> key;
  BasedVector<unsigned, B> pos;
  
public:
  BasedUllmanSet()
    : n(0)
  {}
  BasedUllmanSet(size_t cap)
    : n(0), key(cap), pos(cap)
  {}
  
  size_t capacity() const { return key.size(); }
  size_t size() const { return n; }
  bool empty() const { return n == 0; }
  void clear() { n = 0; }
  void resize(size_t cap)
  {
    key.resize(cap);
    pos.resize(cap);
    n = 0;
  }
  
  bool contains(int k) const
  {
    unsigned p = pos[k];
    return (p < n
            && key[p] == k);
  }
  
  void insert(int k)
  {
    if (contains(k))
      return;
    
    unsigned p = n++;
    key[p] = k;
    pos[k] = p;
  }
  
  void extend(int k)
  {
    assert(!contains(k));
    
    unsigned p = n++;
    key[p] = k;
    pos[k] = p;
  }
  
  void erase(int k)
  {
    if (!contains(k))
      return;
    
    unsigned p = pos[k];
    --n;
    if (p != n)
      {
        int ell = key[n];
        pos[ell] = p;
        key[p] = ell;
      }
  }
  
  int ith(int i)
  {
    assert(i >= 0 && i < (int)n);
    return key[i];
  }
};

using UllmanSet = BasedUllmanSet<0>;
using UllmanSet1 = BasedUllmanSet<1>;

#endif
