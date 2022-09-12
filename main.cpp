#include "zombie.hpp"

int main() {
  Zombie<int> x(3), y(4);
  std::cout << x.unsafe_get() << y.unsafe_get() << std::endl;
  auto z = bindZombie([](int x, int y){return Zombie(x + y);}, x, y);
  std::cout << z.unsafe_get() << std::endl;
  return 0;
}
