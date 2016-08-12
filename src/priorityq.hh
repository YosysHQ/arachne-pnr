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

#ifndef PNR_PRIORITYQQ_HH
#define PNR_PRIORITYQQ_HH

#include <functional>
#include <vector>

template<typename T, typename Comp = std::less<T>>
class PriorityQ
{
public:
  Comp comp;
  std::vector<T> v;
  unsigned n;
  
public:
  PriorityQ() : n(0) {}
  PriorityQ(Comp comp_) : comp(comp_), n(0) {}
  
  size_t size() const { return n; }
  void clear() { n = 0; }
  bool empty() { return n == 0; }
  
  void push(const T &x)
  {
    assert(v.size() >= n);
    if (v.size() == n)
      v.push_back(x);
    else
      v[n] = x;
    ++n;
    std::push_heap(&v[0], &v[n], comp);
  }
  const T &pop()
  {
    assert(n > 0);
    std::pop_heap(&v[0], &v[n], comp);
    --n;
    return v[n];
  }
  const T &top()
  {
    assert(n > 0);
    return v[0];
  }
};

#endif
