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
    Resource(const Resource& r) {
      assert(!r.moved);
    }
    Resource& operator=(const Resource& r) {
      throw;
    }
    Resource& operator=(Resource&& r) {
      throw;
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
      Zombie<Resource> y = bindZombie([](const Resource& x) { return Zombie(Resource()); }, x);
      EXPECT_EQ(destructor_count, last_destructor_count);
      last_destructor_count = destructor_count;
      EXPECT_TRUE(y.evictable());
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

TEST(TockTreeTest, ReversedOrder) {
  tock_tree<int> tt;
  tt.put({2,6}, 1);
  tt.put({1,10}, 2);
  tt.check_invariant();
  EXPECT_EQ(tt.get(5), 1);
  EXPECT_EQ(tt.get(6), 2);
}

TEST(LargestValueLeTest, LVTTest) {
  std::map<int, int> m;
  m.insert({1, 1});
  m.insert({5, 5});
  m.insert({9, 9});
  EXPECT_EQ(largest_value_le(m, 0), m.end());
  EXPECT_EQ(largest_value_le(m, 1)->second, 1);
  EXPECT_EQ(largest_value_le(m, 3)->second, 1);
  EXPECT_EQ(largest_value_le(m, 5)->second, 5);
  EXPECT_EQ(largest_value_le(m, 7)->second, 5);
  EXPECT_EQ(largest_value_le(m, 9)->second, 9);
  EXPECT_EQ(largest_value_le(m, 10)->second, 9);
}
