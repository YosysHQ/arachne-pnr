
#ifndef PNR_BASEDVECTOR_HH
#define PNR_BASEDVECTOR_HH

#include "bstream.hh"

#include <vector>

// FIXME rename vector.hh

template<typename T,
	 typename std::vector<T>::size_type B>
class BasedVector
{
  using underlying_t = std::vector<T>;
  
  underlying_t v;
  
  using value_type = typename underlying_t::value_type;
  using size_type = typename underlying_t::size_type;
  using reference = typename underlying_t::reference;
  using const_reference = typename underlying_t::const_reference;
  using pointer = typename underlying_t::pointer;
  using const_pointer = typename underlying_t::const_pointer;
  using iterator = typename underlying_t::iterator;
  using const_iterator = typename underlying_t::const_iterator;
  using reverse_iterator = typename underlying_t::reverse_iterator;
  using const_reverse_iterator = typename underlying_t::const_reverse_iterator;
  
public:
  BasedVector() {}
  BasedVector(size_type count) : v(count) {}
  BasedVector(size_type count, const T &value) : v(count, value) {}
  BasedVector(std::initializer_list<T> init) : v(init) {}
  
  bool empty() const { return v.empty(); }
  size_type size() const { return v.size(); }
  size_type max_size() const { return v.max_size(); }
  void reserve(size_type capacity) const { v.reserve(capacity); }
  size_type capacity() const { return v.capacity(); }
  void clear() { v.clear(); }
  
  iterator begin() { return v.begin(); }
  const_iterator begin() const { return v.begin(); }
  const_iterator cbegin() const { return v.cbegin(); }
  
  iterator end() { return v.end(); }
  const_iterator end() const { return v.end(); }
  const_iterator cend() const { return v.cend(); }
  
  reverse_iterator rbegin() { return v.rbegin(); }
  const_reverse_iterator rbegin() const { return v.rbegin(); }
  const_reverse_iterator crbegin() const { return v.crbegin(); }
  
  reverse_iterator rend() { return v.rend(); }
  const_reverse_iterator rend() const { return v.rend(); }
  const_reverse_iterator crend() const { return v.crend(); }
  
  reference at(size_type i)
  {
    assert(i >= B && i < B + v.size());
    return v.at(i - B);
  }
  const_reference at(size_type i) const
  {
    assert(i >= B && i < B + v.size());
    return v.at(i - B);
  }
  
  reference operator[](size_type i)
  {
    assert(i >= B && i < B + v.size());
    return v[i - B];
  }
  const_reference operator[](size_type i) const
  {
    assert(i >= B && i < B + v.size());
    return v[i - B];
  }
  
  void push_back(const T &value) { v.push_back(value); }
  void push_back(T &&value) { v.push_back(value); }
  void pop_back() { v.pop_back(); }
  void resize(size_type count) { v.resize(count); }
  void resize(size_type count, const value_type &value) { v.resize(count, value); }
  
  underlying_t &underlying() { return v; }
  const underlying_t &underlying() const { return v; }
};

template<typename T, typename std::vector<T>::size_type B>
obstream &operator<<(obstream &obs, const BasedVector<T, B> &bv)
{
  return obs << bv.underlying();
}

template<typename T, typename std::vector<T>::size_type B>
ibstream &operator>>(ibstream &ibs, BasedVector<T, B> &bv)
{
  return ibs >> bv.underlying();
}

template<typename T>
using Vector = BasedVector<T, 0>;

template<typename T>
using Vector1 = BasedVector<T, 1>;

#endif
