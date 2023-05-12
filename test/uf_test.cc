#include "zombie/zombie.hpp"

#include <gtest/gtest.h>

constexpr ZombieConfig uf_cost_cfg = { default_config.heap, default_config.tree, &uf_cost_metric, {1, 1} };
constexpr ZombieConfig uf_cfg = { default_config.heap, default_config.tree, &uf_metric, {1, 1} };

namespace Local {
  IMPORT_ZOMBIE(default_config)
}

namespace UFCost {
  IMPORT_ZOMBIE(uf_cost_cfg)
}

namespace UF {
  IMPORT_ZOMBIE(uf_cfg)
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
  using namespace UFCost;
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
  using namespace UFCost;
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



TEST(ZombieUFTest, CompareWithLocal) {
#define EXAMPLE_CODE \
    auto& t = Trailokya::get_trailokya();\
    Zombie<int> z1 = bindZombie([&]() {\
      t.meter.fast_forward(100s);\
      return Zombie<int>(1);\
    });\
    Zombie<int> z2 = bindZombie([&](int x1) {\
      t.meter.fast_forward(101s);\
      return Zombie<int>(x1 + 1);\
    }, z1);\
    Zombie<int> z3 = bindZombie([&]() {\
      t.meter.fast_forward(200s);\
      return Zombie<int>(3);\
    });\
    t.reaper.murder();\
    EXPECT_TRUE (z1.evicted());\
    EXPECT_FALSE(z2.evicted() || z3.evicted());\
    t.reaper.murder();

  {
    using namespace Local;
    EXAMPLE_CODE
    EXPECT_TRUE (z2.evicted());
    EXPECT_FALSE(z3.evicted());
  }

  {
    using namespace UFCost;
    EXAMPLE_CODE
    EXPECT_TRUE (z3.evicted());
    EXPECT_FALSE(z2.evicted());
  }
}



template<typename test_id>
struct Resource {
  static unsigned int count;

  int value;
  Resource(int value) : value(value) { ++count; }
  Resource(const Resource&) = delete;
  ~Resource() { --count; }
};

template<typename test_id>
unsigned int Resource<test_id>::count = 0;

template<typename test_id>
struct GetSize<Resource<test_id>> {
  size_t operator()(const Resource<test_id>&) {
    return 1 << 16;
  };
};

// access a list of zombies with linear dependency, first from beginning to end,
// then from end to beginning.
// - total_size: total number of zombies
// - memory_limit: number of zombies alive
// - return value: the list of not evicted zombies after the test
template<typename test_id>
ns LinearDependencyForwardBackwardTest(unsigned int total_size, unsigned int memory_limit) {
  assert(total_size > 1);
  assert(0 < memory_limit && memory_limit <= total_size);

  using namespace UF;
  auto& t = Trailokya::get_trailokya();

  using Resource = Resource<test_id>;

  std::vector<Zombie<Resource>> zs;

  auto allocate_memory = [&]() {
    while (Resource::count >= memory_limit)
      t.reaper.murder();
  };


  ns t0 = t.meter.time();
  zs.push_back(bindZombie([&]() {
    allocate_memory();
    t.meter.fast_forward(100s);
    return Zombie<Resource>(0);
  }));

  // forward
  for (int i = 1; i < total_size; ++i) {
    zs.push_back(bindZombie([&](const Resource& x) {
      allocate_memory();
      t.meter.fast_forward(100s);
      return Zombie<Resource>(x.value + 1);
    }, zs[i - 1]));
  }


  // backward
  for (int i = total_size - 1; i >= 0; --i) {
    t.meter.fast_forward(10s);
    Zombie<Resource> z = zs[i];
    EXPECT_EQ(z.shared_ptr()->get_ref().value, i);
    EXPECT_FALSE(z.evicted());
  }
  ns t1 = t.meter.time();

  while (t.reaper.have_soul())
    t.reaper.murder();

  return t1 - t0;
}






double r_square(const std::vector<double>& data, std::function<double(unsigned int)> f) {
  double x_avg = 0, y_avg = 0, xy_avg = 0, x2_avg = 0, y2_avg = 0;
  for (unsigned int i = 0; i < data.size(); ++i) {
    double x = f(i);
    double y = data[i];
    x_avg += x;
    y_avg += y;
    xy_avg += x * y;
    x2_avg += x * x;
    y2_avg += y * y;
  }
  x_avg /= data.size();
  y_avg /= data.size();
  xy_avg /= data.size();
  x2_avg /= data.size();
  y2_avg /= data.size();

  double nom = xy_avg - x_avg * y_avg;
  return nom * nom / (x2_avg - x_avg * x_avg) / (y2_avg - y_avg * y_avg);
}


TEST(ZombieUFTest, SqrtSpaceLinearTime) {
  struct Test {};

  std::vector<double> times;
  for (int i = 8; i < 20; ++i)
    times.push_back(LinearDependencyForwardBackwardTest<Test>(i * i, 2 * i) / 100s);

  double r2 = r_square(times, [](unsigned int x) { return x; });
  EXPECT_LT(0.9, r2);
  EXPECT_LT(r2, 1.0);
}



TEST(ZombieUFTest, LogSpaceNLogNTime) {
  struct Test {};

  std::vector<double> times;
  for (int i = 8; i < 20; ++i)
    times.push_back(LinearDependencyForwardBackwardTest<Test>(pow(2, 0.5 * i), 2 * i) / 100s);

  // FIXME: currently the actual outcome is exponetial time
  double r2 = r_square(times, [](unsigned int x) {
    double size = 0.5 * (8 + x);
    return pow(2, size);
  });
  EXPECT_LT(0.9, r2);
  EXPECT_LT(r2, 1.0);

}
