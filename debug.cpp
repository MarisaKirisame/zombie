#include "zombie.hpp"

int main() {
  Zombie<int> x(1);
  Zombie<int> y = bindZombie([](int x) { return Zombie(x * 2); }, x);
  Zombie<int> z = bindZombie([](int y) { return Zombie(y * 2); }, y);
  ASSERT(y.evictable());
  y.evict();
  ASSERT(z.evictable());
  z.evict();
  ASSERT(z.get_value() == 4);
}
