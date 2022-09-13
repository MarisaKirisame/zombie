#include "zombie.hpp"

#include <gtest/gtest.h>

TEST(ZombieTest, Create) {
  Zombie<int> x(42);
  EXPECT_EQ(x.unsafe_get(), 42);
}

TEST(ZombieTest, Bind) {
  Zombie<int> x(6), y(7);
  Zombie<int> z = bindZombie([](int x, int y){return Zombie(x * y);}, x, y);
  EXPECT_EQ(z.unsafe_get(), 42);
}
