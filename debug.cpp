#include "test/common.hpp"
#include "zombie/zombie.hpp"

#define EXPECT_EQ(x, y) assert((x) == (y));
#define EXPECT_TRUE(x) assert(x);
#define EXPECT_FALSE(x) assert(!(x));

constexpr ZombieConfig uf_cfg(/*use_cps=*/false, /*metric=*/&uf_metric, /*approx_factor=*/{1, 1});

IMPORT_ZOMBIE(uf_cfg)

template<typename T, typename U>
struct GetSize<std::pair<T, U>> {
  size_t operator()(const std::pair<T, U>& p) {
    return GetSize<T>()(p.first) + GetSize<U>()(p.second);
  };
};

int main() {
  Zombie<int> a(1);
  Zombie<int> b = bindZombieTC<int>([&]() {
    return TailCall([](int x){ return Result(Zombie<int>(x + 1)); }, a);
  });
  EXPECT_EQ(b.get_value(), 2);
  b.evict();
  EXPECT_EQ(b.get_value(), 2);
}
