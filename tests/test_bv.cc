
#include "bitvector.hh"
#include "util.hh"

#include <iostream>
#include <vector>
#include <cassert>

void
test(int n, random_generator &rg)
{
  std::vector<bool> a(n);
  BitVector b(n);
  assert((int)a.size() == n);
  assert((int)b.size() == n);
  
  for (int i = 0; i < n; ++i)
    {
      assert(!a[i]);
      assert(!b[i]);
    }
  
  for (int k = 0; k < 2*n/3; ++k)
    {
      int i = random_int(0, n-1, rg);
      a[i] = true;
      b[i] = true;
    }
  for (int k = 0; k < n/3; ++k)
    {
      int i = random_int(0, n-1, rg);
      a[i] = false;
      b[i] = false;
    }
  
  int k = 0;
  for (int i = 0; i < n; ++i)
    {
      assert(a[i] == b[i]);
      if (b[i])
        k++;
    }
  // std::cout << k << "\n";
  
  int n2 = random_int(0, n, rg);
  a.resize(n2);
  b.resize(n2);
  
  assert((int)a.size() == n2);
  assert((int)b.size() == n2);
  
  for (int i = 0; i < n2; ++i)
    assert(a[i] == b[i]);
  
  b.zero();
  for (int i = 0; i < n2; ++i)
    assert(!b[i]);
}

int
main()
{
  random_generator rg;
  
  for (int n = 0; n <= 1000; ++n)
    test(n, rg);
  test(10000, rg);
}
