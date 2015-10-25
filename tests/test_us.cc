
#include "ullmanset.hh"
#include "util.hh"

#include <set>
#include <iostream>

void
test(int n, random_generator &rg)
{
  std::set<int> a;
  UllmanSet b(n);
  
  assert((int)b.capacity() == n);
  
  for (int i = 0; i < n; ++i)
    {
      assert(!contains(a, i));
      assert(!b.contains(i));
    }
  
  for (int k = 0; k < 2*n/3; ++k)
    {
      int i = random_int(0, n-1, rg);
      a.insert(i);
      b.insert(i);
    }
  assert(a.size() == b.size());
  
  for (int k = 0; k < n/3; ++k)
    {
      int i = random_int(0, n-1, rg);
      a.erase(i);
      b.erase(i);
    }
  assert(a.size() == b.size());
  
  int k = 0;
  for (int i = 0; i < n; ++i)
    {
      assert(contains(a, i) == b.contains(i));
      if (b.contains(i))
        ++k;
    }
  // std::cout << n << " " << k << "\n";
  
  std::set<int> a2;
  for (int i = 0; i < (int)b.size(); ++i)
    a2.insert(b.ith(i));
  assert(a2 == a);
}

int
main()
{
  random_generator rg;
  
  for (int n = 0; n <= 1000; ++n)
    test(n, rg);
  test(10000, rg);
}
