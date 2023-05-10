#include "zombie/zombie.hpp"

#include <gtest/gtest.h>

constexpr ZombieConfig uf_cfg = { default_config.heap, default_config.tree, &uf_cost_metric };

namespace UF {
  IMPORT_ZOMBIE(uf_cfg)
}

namespace Local {
  IMPORT_ZOMBIE(default_config)
}


template<>
struct GetSize<int> {
  size_t operator()(const int&) {
    return sizeof(int);
  }
};

template<typename T, typename U>
struct GetSize<std::pair<T, U>> {
  size_t operator()(const std::pair<T, U>& p) {
    return GetSize<T>()(p.first) + GetSize<U>()(p.second);
  };
};


TEST(ZombieUFTest, CalculateTotalCost) {
  using namespace UF;
  auto& t = Trailokya::get_trailokya();

  Zombie<int> z1 = bindZombie([&]() {
    t.meter.fast_forward(10s);
    return Zombie<int>(1);
  });
  Zombie<int> z2 = bindZombie([&](int x1) {
    t.meter.fast_forward(11s);
    return Zombie<int>(x1 + 1);
  }, z1);


  Zombie<int> z3 = bindZombie([&]() {
    t.meter.fast_forward(20s);
    return Zombie<int>(4);
  });


  t.meter.fast_forward(1s);

  Time z1_cost = Trailokya::get_trailokya().get_microwave(z1.created_time)->cost_of_set();
  Time z2_cost = Trailokya::get_trailokya().get_microwave(z2.created_time)->cost_of_set();
  Time z3_cost = Trailokya::get_trailokya().get_microwave(z3.created_time)->cost_of_set();
  EXPECT_EQ(z1_cost.count() / Time(1s).count(), 10);
  EXPECT_EQ(z2_cost.count() / Time(1s).count(), 11);
  EXPECT_EQ(z3_cost.count() / Time(1s).count(), 20);

  t.reaper.murder();
  EXPECT_TRUE (z1.evicted());
  EXPECT_FALSE(z2.evicted() || z3.evicted());

  z2_cost = Trailokya::get_trailokya().get_microwave(z2.created_time)->cost_of_set();
  z3_cost = Trailokya::get_trailokya().get_microwave(z3.created_time)->cost_of_set();
  EXPECT_EQ(z2_cost.count() / Time(1s).count(), 21);
  EXPECT_EQ(z3_cost.count() / Time(1s).count(), 20);
}


TEST(ZombieUFTest, ReplayChangeCost) {
  using namespace UF;
  auto& t = Trailokya::get_trailokya();

  // z1 -> z2 -> z4 <- z3
  Zombie<int> z1 = bindZombie([&]() {
    t.meter.fast_forward(10s);
    return Zombie<int>(1);
  });
  Zombie<int> z2 = bindZombie([&](int x1) {
    t.meter.fast_forward(11s);
    return Zombie<int>(x1 + 1);
  }, z1);
  Zombie<int> z3 = bindZombie([&]() {
    t.meter.fast_forward(12s);
    return Zombie<int>(3);
  });
  Zombie<int> z4 = bindZombie([&](int x2, int x3) {
    t.meter.fast_forward(14s);
    return Zombie<int>(x2 + x3);
  }, z2, z3);


  auto m1 = t.get_microwave(z1.created_time);
  auto m2 = t.get_microwave(z2.created_time);
  auto m3 = t.get_microwave(z3.created_time);
  auto m4 = t.get_microwave(z4.created_time);


  t.meter.fast_forward(1s);
  for (int i = 0; i < 4; ++i)
    t.reaper.murder();

  EXPECT_TRUE (z2.evicted() && z2.evicted() && z3.evicted() && z4.evicted());

  EXPECT_EQ(m1->cost_of_set().count() / Time(1s).count(), 47);
  EXPECT_EQ(m2->cost_of_set().count() / Time(1s).count(), 47);
  EXPECT_EQ(m3->cost_of_set().count() / Time(1s).count(), 47);
  EXPECT_EQ(m4->cost_of_set().count() / Time(1s).count(), 47);

  int x2 = z2.get_value();
  EXPECT_EQ(x2, 2);
  EXPECT_FALSE(z2.evicted());

  EXPECT_EQ(m3->cost_of_set().count() / Time(1s).count(), 26);
  EXPECT_EQ(m4->cost_of_set().count() / Time(1s).count(), 26);
}



// currently this test fails,
// because aff function in [Trailokya::book] is not correctly updated
TEST(ZombieUFTest, CompareWithLocal) {
  {
    using namespace Local;
    auto& t = Trailokya::get_trailokya();

    Zombie<int> z1 = bindZombie([&]() {
      t.meter.fast_forward(10s);
      return Zombie<int>(1);
    });
    Zombie<int> z2 = bindZombie([&](int x1) {
      t.meter.fast_forward(11s);
      return Zombie<int>(x1 + 1);
    }, z1);


    Zombie<int> z3 = bindZombie([&]() {
      t.meter.fast_forward(20s);
      return Zombie<int>(4);
    });

    t.meter.fast_forward(1s);

    t.reaper.murder();
    EXPECT_TRUE (z1.evicted());
    EXPECT_FALSE(z2.evicted() || z3.evicted());

    t.reaper.murder();
    EXPECT_TRUE (z2.evicted());
    EXPECT_FALSE(z3.evicted());
  }

  {
    using namespace UF;
    auto& t = Trailokya::get_trailokya();

    Zombie<int> z1 = bindZombie([&]() {
      t.meter.fast_forward(10s);
      return Zombie<int>(1);
    });
    Zombie<int> z2 = bindZombie([&](int x1) {
      t.meter.fast_forward(11s);
      return Zombie<int>(x1 + 1);
    }, z1);


    Zombie<int> z3 = bindZombie([&]() {
      t.meter.fast_forward(20s);
      return Zombie<int>(4);
    });


    t.meter.fast_forward(1s);

    t.reaper.murder();
    EXPECT_TRUE (z1.evicted());
    EXPECT_FALSE(z2.evicted() || z3.evicted());

    std::cout << t.book.get_aff(0) << std::endl;
    std::cout << t.book.get_aff(1) << std::endl;
    t.meter.fast_forward(1s);
    t.reaper.murder();
    EXPECT_TRUE (z3.evicted());
    EXPECT_FALSE(z2.evicted());
  }
}
