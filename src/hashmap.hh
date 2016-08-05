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

#ifndef PNR_HASHMAP_HH
#define PNR_HASHMAP_HH

#include <functional>
#include <unordered_map>

template<typename Key,
         typename T,
         typename Hash = std::hash<Key>,
         typename KeyEqual = std::equal_to<Key>>
class HashMap
{
  using underlying_t = std::unordered_map<Key, T, Hash>;
  
  underlying_t m;
  
public:
  using key_type = typename underlying_t::key_type;
  using mapped_type = typename underlying_t::mapped_type;
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
    friend class HashMap;
    typename underlying_t::const_iterator i;
    
public:
    const_iterator() {}
    const_iterator(typename underlying_t::const_iterator i_) : i(i_) {}
    
    bool operator==(const const_iterator &that) { return i == that.i; }
    bool operator!=(const const_iterator &that) { return i != that.i; }
    
    // hash table: cannot modify keys
    const value_type &operator*() { return *i; }
    const value_type &operator*() const { return *i; }
    const value_type *operator->() { return i.operator->(); }
    const value_type *operator->() const { return i.operator->(); }
    
    // not incrementable
  };
  
  using iterator = const_iterator;
  
public:
  HashMap() {}
  
  iterator begin() { return iterator(m.begin()); }
  const_iterator begin() const { return const_iterator(m.begin()); }
  const_iterator cbegin() const { return const_iterator(m.cbegin()); }
  
  iterator end() { return iterator(m.end()); }
  const_iterator end() const { return const_iterator(m.end()); }
  const_iterator cend() const { return const_iterator(m.cend()); }
  
  bool empty() const { return m.empty(); }
  
  size_type size() const { return m.size(); }
  size_type max_size() const { return m.max_size(); }
  
  iterator erase(const_iterator pos) { return iterator(m.erase(pos.i)); }
  // no first, last: can't iterate
  size_type erase(const key_type &key) { return m.erase(key); }
  
  void clear() { return m.clear(); }
  
  std::pair<iterator,bool> insert(const value_type &value) 
  {
    const auto &p = m.insert(value);
    return std::make_pair(iterator(p.first), p.second);
  }
  template<typename P>
  std::pair<iterator,bool> insert(P &&value)
  {
    const auto &p = m.insert(value);
    return std::make_pair(iterator(p.first), p.second);
  }
  std::pair<iterator,bool> insert(value_type &&value) 
  {
    const auto &p = m.insert(value);
    return std::make_pair(iterator(p.first), p.second);
  }
  iterator insert(const_iterator hint, const value_type &value)
  {
    return iterator(m.insert(hint, value));
  }
  template<typename P>
  iterator insert(const_iterator hint, P &&value)
  {
    return iterator(m.insert(hint, value));
  }
  iterator insert(const_iterator hint, value_type &&value)
  {
    return iterator(m.insert(hint, value));
  }
  template<class InputIt>
  void insert(InputIt first, InputIt last) { m.insert(first, last); }
  void insert(std::initializer_list<value_type> ilist) { return m.insert(ilist); }
  
  T &operator[](const Key &key) { return m[key]; }
  T &operator[](Key &&key) { return m[key]; }
  
  iterator find(const Key &key) { return iterator(m.find(key)); }
  const_iterator find(const Key &key) const { return const_iterator(m.find(key)); }
  
  T &at(const Key &key) { return m.at(key); }
  const T &at(const Key &key) const { return m.at(key); }
  
  size_type count(const key_type &key) const { return m.key(); }
  
  bool operator ==(const HashMap &hs) { return m == hs.m; }
  bool operator !=(const HashMap &hs) { return m != hs.m; }
  
  underlying_t &underlying() { return m; }
  const underlying_t &underlying() const { return m; }
};



#endif
