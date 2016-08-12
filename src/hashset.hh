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

#ifndef PNR_HASHSET_HH
#define PNR_HASHSET_HH

#include <functional>
#include <unordered_set>

template<typename T> struct Hash;

template<typename T,
         typename Hash = std::hash<T>,
         typename KeyEqual = std::equal_to<T>>
class HashSet
{
  using underlying_t = std::unordered_set<T, Hash>;
  
  underlying_t s;
  
public:
  using key_type = typename underlying_t::key_type;
  using value_type = typename underlying_t::value_type;
  using size_type = typename underlying_t::size_type;
  using difference_type = typename underlying_t::difference_type;
  using hasher = typename underlying_t::hasher;
  using key_equal = typename underlying_t::key_equal;
  using allocator_type = typename underlying_t::allocator_type;
  using reference = typename underlying_t::reference;
  using const_reference = typename underlying_t::const_reference;
  using pointer = typename underlying_t::pointer;
  using const_pointer = typename underlying_t::const_pointer;
  
  class const_iterator
  {
    friend class HashSet;
    typename underlying_t::const_iterator i;
    
public:
    const_iterator() {}
    const_iterator(typename underlying_t::const_iterator i_) : i(i_) {}
    
    bool operator==(const const_iterator &that) { return i == that.i; }
    bool operator!=(const const_iterator &that) { return i != that.i; }
    
    // hash table: cannot modify keys
    const value_type &operator*() { return *i; }
    const value_type &operator*() const { return *i; }
    const value_type *operator->() { return operator->(i); }
    const value_type *operator->() const { return operator->(i); }
    
    // not incrementable
  };
  
  using iterator = const_iterator;
  
public:
  HashSet() {}
  
  iterator begin() { return iterator(s.begin()); }
  const_iterator begin() const { return const_iterator(s.begin()); }
  const_iterator cbegin() const { return const_iterator(s.cbegin()); }
  
  iterator end() { return iterator(s.end()); }
  const_iterator end() const { return const_iterator(s.end()); }
  const_iterator cend() const { return const_iterator(s.cend()); }
  
  bool empty() const { return s.empty(); }
  
  size_type size() const { return s.size(); }
  size_type max_size() const { return s.max_size(); }
  
  iterator erase(const_iterator pos) { return iterator(s.erase(pos.i)); }
  // no first, last: can't iterate
  size_type erase(const key_type &key) { return s.erase(key); }
  
  void clear() { return s.clear(); }
  
  std::pair<iterator,bool> insert(const value_type &value) 
  {
    const auto &p = s.insert(value);
    return std::make_pair(iterator(p.first), p.second);
  }
  std::pair<iterator,bool> insert(value_type &&value) 
  {
    const auto &p = s.insert(value);
    return std::make_pair(iterator(p.first), p.second);
  }
  iterator insert(const_iterator hint, const value_type &value)
  {
    return iterator(s.insert(hint, value));
  }
  iterator insert(const_iterator hint, value_type &&value)
  {
    return iterator(s.insert(hint, value));
  }
  template<class InputIt>
  void insert(InputIt first, InputIt last) { s.insert(first, last); }
  void insert(std::initializer_list<value_type> ilist) { return s.insert(ilist); }
  
  size_type count(const key_type &key) const { return s.key(); }
  
  bool operator ==(const HashSet &hs) { return s == hs.s; }
  bool operator !=(const HashSet &hs) { return s != hs.s; }
  
  underlying_t &underlying() { return s; }
  const underlying_t &underlying() const { return s; }
};

#endif
