#include "Zombie/zombie.hpp"

int main() {
  tock_tree<int> tt;
  tt.put({2,6}, 1);
  tt.put({1,10}, 2);
  tt.check_invariant();
  ASSERT(tt.get(5) == 1);
  ASSERT(tt.get(6) == 2);
}
