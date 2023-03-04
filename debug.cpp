#include "Zombie/zombie.hpp"

int main() {
  Zombie<int> x(3);
  auto y = bindZombie([](const int& x) { return Zombie(x * 2); }, x);
  y.force_unique_evict();
  ASSERT(y.get_value() == 6);
  ASSERT(y.evictable());
  y.force_unique_evict();
  ASSERT(y.get_value() == 6);
}
