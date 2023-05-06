#include "zombie/zombie.hpp"

#include <gtest/gtest.h>

IMPORT_ZOMBIE(default_config)

template<>
struct GetSize<int> {
  size_t operator()(const int&) {
    return sizeof(int);
  }
};

TEST(ZombieTest, Create) {
  Zombie<int> x(42);
  EXPECT_EQ(x.get_value(), 42);
}

TEST(ZombieTest, Bind) {
  Zombie<int> x(6), y(7);
  Zombie<int> z = bindZombie([](int x, int y) { return Zombie<int>(x * y); }, x, y);
  EXPECT_EQ(z.get_value(), 42);
}

TEST(ZombieTest, BindUnTyped) {
  Zombie<int> x(6), y(7);
  Zombie<int> z = bindZombieUnTyped([](const std::vector<const void*>& vec) {
    return Zombie<int>(*static_cast<const int*>(vec[0]) * *static_cast<const int*>(vec[1]));
  }, {x, y});
  EXPECT_EQ(z.get_value(), 42);
}

static int destructor_count = 0;
int last_destructor_count = 0;
struct Resource {
  Resource() = default;
  Resource(Resource&& r) = delete;
  Resource(const Resource& r) = delete;
  ~Resource() {
    ++destructor_count;
  }
};
template<>
struct GetSize<Resource> {
  size_t operator()(const Resource&) {
    return 1;
  }
};

TEST(ZombieTest, Resource) {
  Zombie<Resource> x;
  EXPECT_EQ(destructor_count, 0);
  {
    last_destructor_count = destructor_count;
    Zombie<Resource> y = bindZombie([](const Resource& x) { return Zombie<Resource>(); }, x);
    assert(destructor_count == last_destructor_count);
    last_destructor_count = destructor_count;
    y.force_unique_evict();
    EXPECT_EQ(destructor_count, last_destructor_count+1) << "evict does not release resource";
    last_destructor_count = destructor_count;
  }
  EXPECT_EQ(destructor_count, last_destructor_count) << "evicted value does not get destructed again";
}

// TODO: test for eagereviction
// TODO: test for cleanup

TEST(ZombieTest, SourceNoEvict) {
  Zombie<int> x(3);
  EXPECT_FALSE(x.evictable());
}

TEST(ZombieTest, Recompute) {
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

TEST(ZombieTest, ChainRecompute) {
  Zombie<int> x(1);
  Zombie<int> y = bindZombie([](int x) { return Zombie<int>(x * 2); }, x);
  Zombie<int> z = bindZombie([](int y) { return Zombie<int>(y * 2); }, y);
  y.force_unique_evict();
  z.force_unique_evict();
  EXPECT_EQ(z.get_value(), 4);
}

TEST(ZombieTest, ChainRecomputeDestructed) {
  Zombie<int> x(1);
  Zombie<int> z = [&](){
    Zombie<int> y = bindZombie([](int x) { return Zombie<int>(x * 2); }, x);
    return bindZombie([](int y) { return Zombie<int>(y * 2); }, y);
  }();
  z.force_unique_evict();
  EXPECT_EQ(z.get_value(), 4);
}

TEST(ZombieTest, DiamondRecompute) {
  Zombie<int> a(1);
  size_t executed_time = 0;
  Zombie<int> b = bindZombie([&](int x) {
    ++executed_time;
    return Zombie<int>(x * 2);
  }, a);
  Zombie<int> c = bindZombie([](int x) { return Zombie<int>(x * 2); }, b);
  Zombie<int> d = bindZombie([](int x) { return Zombie<int>(x * 2); }, b);
  Zombie<int> e = bindZombie([](int x, int y) { return Zombie<int>(x + y); }, c, d);
  EXPECT_EQ(executed_time, 1);
  b.force_unique_evict();
  c.force_unique_evict();
  d.force_unique_evict();
  e.force_unique_evict();
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
    return Zombie<int>(x * 2);
  }, a);
  Zombie<int> c = bindZombie([&](int x) {
    return Zombie<int>(x * 2);
  }, b);
  Zombie<int> d = bindZombie([&](int x) {
    b.force_unique_evict();
    return Zombie<int>(x * 2);
  }, c);
  Zombie<int> e = bindZombie([](int x) { return Zombie<int>(x * 2); }, b);
  Zombie<int> f = bindZombie([](int x, int y) { return Zombie<int>(x + y); }, d, e);
  executed_time = 0;
  assert(b.evictable());
  b.force_unique_evict();
  c.force_unique_evict();
  d.force_unique_evict();
  e.force_unique_evict();
  f.force_unique_evict();
  EXPECT_EQ(f.get_value(), 12);
  EXPECT_EQ(executed_time, 2);
}

TEST(ZombieTest, ZombieRematWithSmallestFunction) {
  // While recursive test seems very contrived,
  // recursive MicroWave happend when your value is recursive.
  // In such a case, the Zombie will contain more Zombie.
  // TODO: test some recursive structure and function.
  static size_t outer_executed_time = 0;
  static size_t inner_executed_time = 0;
  Zombie<int> x = bindZombie(
    []() {
      ++outer_executed_time;
      return bindZombie(
        []() {
	  ++inner_executed_time;
	  return Zombie<int>(42);
	});
    });
  EXPECT_EQ(outer_executed_time, 1);
  EXPECT_EQ(inner_executed_time, 1);
  EXPECT_EQ(x.get_value(), 42);
  EXPECT_EQ(outer_executed_time, 1);
  EXPECT_EQ(inner_executed_time, 1);
  x.force_unique_evict();
  EXPECT_EQ(outer_executed_time, 1);
  EXPECT_EQ(inner_executed_time, 1);
  EXPECT_EQ(x.get_value(), 42);
  EXPECT_EQ(outer_executed_time, 1);
  EXPECT_EQ(inner_executed_time, 2);
}

TEST(ZombieTest, SkipZombieAliveRecursiveFunction) {
  static size_t y_executed_time = 0;
  static size_t z_executed_time = 0;
  Zombie<int> x(1);
  Zombie<int> z = bindZombie(
    [](int x) {
      ++z_executed_time;
      Zombie<int> y = bindZombie([=]() {
        ++y_executed_time;
        return Zombie<int>(2);
      });
      return Zombie<int>(3);
    }, x);
  EXPECT_EQ(z.get_value(), 3);
  EXPECT_EQ(y_executed_time, 1);
  EXPECT_EQ(z_executed_time, 1);
  z.force_unique_evict();
  EXPECT_EQ(z.get_value(), 3);
  EXPECT_EQ(y_executed_time, 1);
  EXPECT_EQ(z_executed_time, 2);
}

TEST(ZombieTest, Copy) {
  Zombie<int> x(1);
  Zombie<int> y = x;
  EXPECT_EQ(x.get_value(), y.get_value());
}

TEST(ZombieTest, StoreReturn) {
  Zombie<int> a(1);
  Zombie<int> b = bindZombie([&](int a){
    Zombie<int> c(2);
    Zombie<int> d(3);
    return c;
  }, a);
  EXPECT_EQ(b.get_value(), 2);
  b.force_unique_evict();
  EXPECT_EQ(b.get_value(), 2);
}

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

TEST(ZombieTest, Reaper) {
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
  EXPECT_FALSE(a.evicted());
  EXPECT_FALSE(b.evicted());
  EXPECT_TRUE(c.evicted());
  EXPECT_FALSE(d.evicted());
}



template<typename T, typename U>
struct GetSize<std::pair<T, U>> {
  size_t operator()(const std::pair<T, U>& p) {
    return GetSize<T>()(p.first) + GetSize<U>()(p.second);
  };
};

template<typename T>
struct GetSize<Zombie<T>> {
  size_t operator()(const Zombie<T>&) {
    return sizeof(Zombie<T>);
  };
};

TEST(ZombieTest, EvictByMicroWave) {
  size_t MB_in_bytes = 1 >> 19;

  auto z = bindZombie([&]() {
    Zombie<Block> a(MB_in_bytes);
    Zombie<Block> b(MB_in_bytes);
    return Zombie<std::pair<Zombie<Block>, Zombie<Block>>>{ a, b };
  });
  Zombie<Block> a = z.get_value().first;
  Zombie<Block> b = z.get_value().second;

  Trailokya::get_trailokya().meter.fast_forward(1s);
  Trailokya::get_trailokya().reaper.murder();

  EXPECT_TRUE(a.evicted());
  EXPECT_TRUE(b.evicted());
}


TEST(ZombieTest, MeasureSpace) {
  {
    Zombie<int> z = bindZombie([&]() {
      Zombie<int> za(1);
      return bindZombie([&](int a) {
        Zombie<int> b(a + 1);
        return za;
      }, za);
    });


    z.evict();
    EXPECT_FALSE(Trailokya::get_trailokya().akasha.has_precise(z.created_time));
    auto value = Trailokya::get_trailokya().akasha.get_node(z.created_time).value;
    EXPECT_EQ(value.index(), ZombieInternal::TockTreeElemKind::MicroWave);
    auto& m = std::get<ZombieInternal::TockTreeElemKind::MicroWave>(value);
    EXPECT_EQ(m->space_taken.count(), Space(sizeof(int)).count());
  }

  {
    Zombie<int> z = bindZombie([&]() {
      Zombie<int> za(1);
      Zombie<int> zb(2);
      return zb;
    });

    z.evict();
    EXPECT_FALSE(Trailokya::get_trailokya().akasha.has_precise(z.created_time));
    auto value = Trailokya::get_trailokya().akasha.get_node(z.created_time).value;
    EXPECT_EQ(value.index(), ZombieInternal::TockTreeElemKind::MicroWave);
    auto& m = std::get<ZombieInternal::TockTreeElemKind::MicroWave>(value);
    EXPECT_EQ(m->space_taken.count(), Space(2 * sizeof(int)).count());
  }
}
