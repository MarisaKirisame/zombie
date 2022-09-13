#include "zombie.hpp"

#include <gtest/gtest.h>

TEST(ZombieTest, Create) {
  Zombie<int> x(42);
  EXPECT_EQ(x.unsafe_get(), 42);
}

TEST(ZombieTest, Bind) {
  Zombie<int> x(6), y(7);
  Zombie<int> z = bindZombie([](int x, int y) { return Zombie(x * y); }, x, y);
  EXPECT_EQ(z.unsafe_get(), 42);
}

TEST(ZombieTest, Resource) {
  static int destructor_count = 0;
  int last_destructor_count = 0;
  struct Resource {
    bool moved = false;
    Resource() = default;
    Resource(Resource&& r) {
      r.moved = true;
    }
    ~Resource() {
      if (!moved) {
        ++destructor_count;
      }
    }
  };
  {
    Zombie<Resource> x;
    EXPECT_EQ(destructor_count, 0);
    {
      last_destructor_count = destructor_count;
      Zombie<Resource> y = bindZombie([](const Resource& x) { return Resource(); }, x);
      EXPECT_EQ(destructor_count, last_destructor_count);
      last_destructor_count = destructor_count;
      EXPECT_EQ(y.evictable(), true);
      y.evict();
      EXPECT_EQ(destructor_count, last_destructor_count+1) << "evict does not release resource";
      last_destructor_count = destructor_count;
    }
    EXPECT_EQ(destructor_count, last_destructor_count) << "evicted value does not get destructed again";
    last_destructor_count = destructor_count;
  }
  EXPECT_EQ(destructor_count, last_destructor_count+1);
}

TEST(ZombieTest, Recompute) {
  Zombie<int> x(3);
  // maybe weird that we allow evicting on input, but input can live out of scope so we have to recompute them anyway.
  x.evict();
  EXPECT_EQ(x.get_value(), 3);
}
