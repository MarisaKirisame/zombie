#include "zombie.hpp"

#include <gtest/gtest.h>
#include "assert.hpp"

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
      ASSERT(destructor_count == last_destructor_count);
      last_destructor_count = destructor_count;
      y.force_evict();
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
  x.force_evict();
  EXPECT_EQ(x.get_value(), 3);
}

TEST(ZombieTest, ChainRecompute) {
  Zombie<int> x(1);
  Zombie<int> y = bindZombie([](int x) { return Zombie(x * 2); }, x);
  Zombie<int> z = bindZombie([](int y) { return Zombie(y * 2); }, y);
  y.force_evict();
  z.force_evict();
  EXPECT_EQ(z.get_value(), 4);
}

TEST(ZombieTest, ChainRecomputeDestructed) {
  Zombie<int> x(1);
  Zombie<int> z = [&](){
    Zombie<int> y = bindZombie([](int x) { return Zombie(x * 2); }, x);
    return bindZombie([](int y) { return Zombie(y * 2); }, y);
  }();
  z.force_evict();
  EXPECT_EQ(z.get_value(), 4);
}

TEST(ZombieTest, RecursiveEvictedRecompute) {
  // During Recompute, we might run out of memory to hold the newly computed value.
  // When that happend, Zombie might evict some value that it recursively recomputed,
  // but is still needed later, in the same recompute chain.
  // In another word, during a single recomputation,
  // recursive values may be recomputed more then once!
  // This precisely enable TREEVERSE.

  // The following test had been very carefully constructed.
  // b is the value where we multi-recompute.
  // we simulate running out of memory by making c evict b.
  // d also depend on b, and e depend on b, c.
  // when we evict everything, to recompute e,
  // we recompute c which recompute and immediately evict b,
  // then we recompute d which recompute b,
  // totalling 2 execution of b in recompute of e.
  Zombie<int> a(1);
  size_t executed_time = 0;
  Zombie<int> b = bindZombie([&](int x) {
    ++executed_time;
    return Zombie(x * 2);
  }, a);
  Zombie<int> c = bindZombie([&](int x) {
    b.force_evict();
    return Zombie(x * 2);
  }, b);
  Zombie<int> d = bindZombie([](int x) { return Zombie(x * 2); }, b);
  Zombie<int> e = bindZombie([](int x, int y) { return Zombie(x + y); }, c, d);
  executed_time = 0;
  b.force_evict();
  c.force_evict();
  d.force_evict();
  e.force_evict();
  EXPECT_EQ(e.get_value(), 8);
  EXPECT_EQ(executed_time, 2);
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
