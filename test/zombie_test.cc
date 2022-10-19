#include "zombie.hpp"

#include <gtest/gtest.h>
#include "assert.hpp"

TEST(ZombieTest, Create) {
  Zombie<int> x(42);
  EXPECT_EQ(x.get_value(), 42);
}

TEST(ZombieTest, Bind) {
  Zombie<int> x(6), y(7);
  Zombie<int> z = bindZombie([](int x, int y) { return Zombie(x * y); }, x, y);
  EXPECT_EQ(z.get_value(), 42);
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
      //y.force_evict();
      EXPECT_EQ(destructor_count, last_destructor_count+1) << "evict does not release resource";
      last_destructor_count = destructor_count;
    }
    EXPECT_EQ(destructor_count, last_destructor_count) << "evicted value does not get destructed again";
    last_destructor_count = destructor_count;
  }
  EXPECT_EQ(destructor_count, last_destructor_count+1);
}


/*
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

TEST(ZombieTest, DiamondRecompute) {
  Zombie<int> a(1);
  size_t executed_time = 0;
  Zombie<int> b = bindZombie([&](int x) {
    ++executed_time;
    return Zombie(x * 2);
  }, a);
  Zombie<int> c = bindZombie([](int x) { return Zombie(x * 2); }, b);
  Zombie<int> d = bindZombie([](int x) { return Zombie(x * 2); }, b);
  Zombie<int> e = bindZombie([](int x, int y) { return Zombie(x + y); }, c, d);
  EXPECT_EQ(executed_time, 1);
  b.force_evict();
  c.force_evict();
  d.force_evict();
  e.force_evict();
  EXPECT_EQ(e.get_value(), 8);
  EXPECT_EQ(executed_time, 2);
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
  // we simulate running out of memory by making d in d <- c <- b evict b.
  // e also depend on b, and f depend on d, e.
  // when we evict everything, to recompute e,
  // we recompute d <- c <- b which evict b,
  // then we recompute e <- b,
  // totalling 2 execution of b in recompute of e.
  Zombie<int> a(1);
  size_t executed_time = 0;
  // The use of capture by reference for b and c is not safe,
  // but we know what we are doing.
  Zombie<int> b = bindZombie([&](int x) {
    ++executed_time;
    return Zombie(x * 2);
  }, a);
  Zombie<int> c = bindZombie([&](int x) {
    return Zombie(x * 2);
  }, b);
  Zombie<int> d = bindZombie([&](int x) {
    b.force_evict();
    return Zombie(x * 2);
  }, c);
  Zombie<int> e = bindZombie([](int x) { return Zombie(x * 2); }, b);
  Zombie<int> f = bindZombie([](int x, int y) { return Zombie(x + y); }, d, e);
  executed_time = 0;
  b.force_evict();
  c.force_evict();
  d.force_evict();
  e.force_evict();
  f.force_evict();
  EXPECT_EQ(f.get_value(), 12);
  EXPECT_EQ(executed_time, 2);
}

TEST(ZombieTest, Lock) {
  Zombie<int> a(0);
  ASSERT(a.evictable());
  bindZombie([&](int a_){
    ASSERT(!a.evictable());
    return Zombie(1);
  }, a);
  ASSERT(a.evictable());
}
*/
