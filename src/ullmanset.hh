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

class UllmanSet
{
  int n;
  std::vector<int> key;
  std::vector<int> pos;
  
public:
  UllmanSet()
    : n(0)
  {}
  UllmanSet(int cap)
    : n(0), key(cap), pos(cap)
  {}
  
  int capacity() const { return key.size(); }
  int size() const { return n; }
  
  bool contains(int k) const
  {
    assert(k >= 0 && k < capacity());
    int p = pos[k];
    return (p < n
	    && key[p] == k);
  }
  
  void insert(int k) const
  {
    if (contains(k))
      return;
    
    p = n++;
    key[p] = k;
    pos[k] = p;
  }

  void erase(int k) const
  {
    if (!contains(k))
      return;
    
    int p = pos[k];
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
    assert(i >= 0 && i < n);
    return key[i];
  }
};
