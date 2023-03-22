#include "zombie/zombie.hpp"

int main() {
  Zombie<int> x(3);
  auto y = bindZombie([](const int& x) { return Zombie(x * 2); }, x);
  y.force_unique_evict();
  assert(y.get_value() == 6);
  assert(y.evictable());
  y.force_unique_evict();
  assert(y.get_value() == 6);
}
