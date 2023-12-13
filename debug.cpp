#include "test/common.hpp"
#include "zombie/zombie.hpp"

#define EXPECT_EQ(x, y) assert((x) == (y));
#define EXPECT_TRUE(x) assert(x);
#define EXPECT_FALSE(x) assert(!(x));

constexpr ZombieConfig local_cfg = { default_config.tree, &local_metric, {1, 1} };
constexpr ZombieConfig uf_cfg = { default_config.tree, &uf_metric, {1, 1} };

namespace Local {
  IMPORT_ZOMBIE(default_config)
}

namespace UF {
  IMPORT_ZOMBIE(uf_cfg)
}

template<typename T, typename U>
struct GetSize<std::pair<T, U>> {
  size_t operator()(const std::pair<T, U>& p) {
    return GetSize<T>()(p.first) + GetSize<U>()(p.second);
  };
};

int main() {
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
  std::cout << "here" << std::endl;

  EXPECT_TRUE (z1.evicted());
  EXPECT_FALSE(z2.evicted() || z3.evicted());

  z2_cost = Trailokya::get_trailokya().get_microwave(z2.created_time)->cost_of_set();
  z3_cost = Trailokya::get_trailokya().get_microwave(z3.created_time)->cost_of_set();
  EXPECT_EQ(z2_cost.count() / Time(1s).count(), 21);
  EXPECT_EQ(z3_cost.count() / Time(1s).count(), 20);
}
