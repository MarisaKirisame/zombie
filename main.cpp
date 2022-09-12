#include "zombie.hpp"

int main() {
  auto x = mkZombie(3), y = mkZombie(4);
  std::cout << x.node->get() << y.node->get() << std::endl;
  auto z = bindZombie([](int x, int y){return mkZombie(x + y);}, x, y);
  std::cout << z.node->get() << std::endl;
  return 0;
}
