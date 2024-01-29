#include "test/common.hpp"
#include "zombie/zombie.hpp"

#define EXPECT_EQ(x, y) assert((x) == (y));
#define EXPECT_TRUE(x) assert(x);
#define EXPECT_FALSE(x) assert(!(x));

IMPORT_ZOMBIE(default_config)

int main() {
  Zombie<int> x(3);
  auto y = bindZombie([](const int& x) { return Zombie<int>(x * 2); }, x);
  EXPECT_FALSE(y.evicted());
  y.force_unique_evict();
  EXPECT_TRUE(y.evicted());
  EXPECT_EQ(y.get_value(), 6);
  assert(y.evictable());
  y.force_unique_evict();
  EXPECT_EQ(y.get_value(), 6);
}
