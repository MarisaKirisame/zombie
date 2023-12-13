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
#define EXAMPLE_CODE                            \
  auto& t = Trailokya::get_trailokya();         \
  Zombie<int> z1 = bindZombie([&]() {           \
    t.meter.fast_forward(100s);                 \
    return Zombie<int>(1);                      \
  });                                           \
  Zombie<int> z2 = bindZombie([&](int x1) {     \
    t.meter.fast_forward(101s);                 \
    return Zombie<int>(x1 + 1);                 \
  }, z1);                                       \
  Zombie<int> z3 = bindZombie([&]() {           \
    t.meter.fast_forward(150s);                 \
    return Zombie<int>(3);                      \
  });                                           \
  t.reaper.murder();                            \
  EXPECT_TRUE (z1.evicted());                   \
  EXPECT_FALSE(z2.evicted() || z3.evicted());   \
  t.reaper.murder();

  {
    using namespace Local;
    EXAMPLE_CODE;
    EXPECT_TRUE (z2.evicted());
    EXPECT_FALSE(z3.evicted());
  }

  {
    using namespace UF;
    EXAMPLE_CODE;
    EXPECT_TRUE (z3.evicted());
    EXPECT_FALSE(z2.evicted());
  }
}
