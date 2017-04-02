/* Copyright (c) 2015-2018 Cotton Seed
   
   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without
   restriction, including without limitation the rights to use, copy,
   modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   
   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE. */

#ifndef PNR_ULLMANSET_HH
#define PNR_ULLMANSET_HH

#include "vector.hh"

#include <cstddef>
#include <cassert>

class UllmanSet
{
  size_t n;
  std::vector<int> key;
  std::vector<unsigned> pos;
  
public:
  UllmanSet()
    : n(0)
  {}
  UllmanSet(size_t cap)
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

#endif
