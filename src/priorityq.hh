
#include <functional>
#include <vector>

template<typename T, typename Comp = std::less<T>>
class PriorityQ
{
public:
  Comp comp;
  std::vector<T> v;
  int n;
  
public:
  PriorityQ() : n(0) {}
  PriorityQ(Comp comp_) : comp(comp_), n(0) {}
  
  int size() const { return n; }
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
