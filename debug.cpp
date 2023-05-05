#include "zombie/zombie.hpp"


IMPORT_ZOMBIE(default_config)

struct Block {
  size_t size;
  Block(size_t size) : size(size) { }
};

template<>
struct GetSize<Block> {
  size_t operator()(const Block& b) {
    return b.size;
  }
};

int main() {
  size_t MB_in_bytes = 1 >> 19;

  Zombie<Block> a(MB_in_bytes);
  Zombie<Block> b = bindZombie([&](const Block& a) {
    Trailokya::get_trailokya().meter.fast_forward(1s);
    return Zombie<Block>(MB_in_bytes);
  }, a);
  Zombie<Block> c = bindZombie([&](const Block& a) {
    Trailokya::get_trailokya().meter.fast_forward(1s);
    return Zombie<Block>(MB_in_bytes);
  }, a);
  Zombie<Block> d = bindZombie([&](const Block& a) {
    Trailokya::get_trailokya().meter.fast_forward(1s);
    return Zombie<Block>(MB_in_bytes);
  }, a);
  Trailokya::get_trailokya().meter.fast_forward(1s);
  b.get_value();
  Trailokya::get_trailokya().reaper.murder();
  assert(!a.evicted());
  assert(!b.evicted());
  assert(c.evicted());
  assert(!d.evicted());
}
