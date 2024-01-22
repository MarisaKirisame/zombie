#include "common.hpp"
#include "zombie/zombie.hpp"

#include <gtest/gtest.h>

IMPORT_ZOMBIE(default_config)

TEST(CPSTest, Create) {
  Zombie<int> x(42);
  EXPECT_EQ(x.get_value(), 42);
}

TEST(CPSTest, Bind) {
  Zombie<int> x(6), y(7);
  Zombie<int> z = bindZombie([](int x, int y) { return Zombie<int>(x * y); }, x, y);
  EXPECT_EQ(z.get_value(), 42);
}

TEST(CPSTest, BindUnTyped) {
  Zombie<int> x(6), y(7);
  Zombie<int> z = bindZombieUnTyped([](const std::vector<const void*>& vec) {
    return Zombie<int>(*static_cast<const int*>(vec[0]) * *static_cast<const int*>(vec[1]));
  }, {x, y});
  EXPECT_EQ(z.get_value(), 42);
}

