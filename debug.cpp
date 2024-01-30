#include "test/common.hpp"
#include "zombie/zombie.hpp"

#define EXPECT_EQ(x, y) assert((x) == (y));
#define EXPECT_TRUE(x) assert(x);
#define EXPECT_FALSE(x) assert(!(x));

IMPORT_ZOMBIE(default_config)

int main() {
  Zombie<int> a(1);
  Zombie<int> b = bindZombieTC([&]() {
    return TailCall([](int x){ return Result(Zombie<int>(x + 1)); }, a);
  });
  EXPECT_EQ(b.get_value(), 2);
  b.evict();
  EXPECT_EQ(b.get_value(), 2);
}
