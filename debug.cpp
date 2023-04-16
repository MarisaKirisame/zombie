#include "zombie/zombie.hpp"
#include "zombie/kinetic.hpp"
#include "zombie/time.hpp"

using namespace std::chrono_literals;

template<>
struct GetSize<int> {
  Space operator()(const int&) {
    return sizeof(int);
  }
};

int main() {
  Zombie<int> x(1);
  Zombie<int> y = bindZombie([](int x) { return Zombie(x * 2); }, x);
  Zombie<int> z = bindZombie([](int y) { return Zombie(y * 2); }, y);
  y.force_unique_evict();
  z.force_unique_evict();
  assert(z.get_value() == 4);
}
