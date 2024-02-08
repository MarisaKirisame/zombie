#include "test/common.hpp"
#include "zombie/zombie.hpp"

#define TEST(L, R) void R()
#define EXPECT_EQ(x, y) assert((x) == (y));
#define EXPECT_LT(x, y) assert((x) < (y));
#define EXPECT_TRUE(x) assert(x);
#define EXPECT_FALSE(x) assert(!(x));

constexpr ZombieConfig local_cfg(/*metric=*/&local_metric, /*approx_factor=*/{1, 1});
constexpr ZombieConfig uf_cfg(/*metric=*/&uf_metric, /*approx_factor=*/{1, 1});

namespace Local {
  IMPORT_ZOMBIE(local_cfg)
}

namespace UnionFind {
  IMPORT_ZOMBIE(uf_cfg)
}

namespace Default {
  IMPORT_ZOMBIE(default_config)
}

// access a list of zombies with linear dependency, first from beginning to end,
// then from end to beginning.
// - total_size: total number of zombies
// - memory_limit: number of zombies alive
// - return value: the total count of computations, including rematerialization
template<typename test_id>
unsigned int LinearDependencyForwardBackwardTest(unsigned int total_size, unsigned int memory_limit) {
  assert(total_size > 1);
  assert(0 < memory_limit);
  //assert(memory_limit <= total_size);

  using namespace UnionFind;
  //using namespace Local;
  auto& t = Trailokya::get_trailokya();

  using Resource = Resource<test_id>;

  std::vector<Zombie<Resource>> zs;

  auto allocate_memory = [&]() {
    while (Resource::count >= memory_limit) {
      t.reaper.murder();
    }
  };

  unsigned int work_done = 0;
  zs.push_back(bindZombie([&]() {
    allocate_memory();
    t.meter.fast_forward(1ms);
    ++work_done;
    return Zombie<Resource>(0);
  }));

  // forward
  for (int i = 1; i < total_size; ++i) {
    zs.push_back(bindZombie([&](const Resource& x) {
      allocate_memory();
      t.meter.fast_forward(1ms);
      ++work_done;
      return Zombie<Resource>(x.value + 1);
    }, zs[i - 1]));
  }

  // backward
  for (int i = total_size - 1; i >= 0; --i) {
    int gap = 0;
    for (const auto& x: zs) {
      if (x.evicted()) {
        ++gap;
      } else {
        std::cout << gap << ", ";
        gap = 0;
      }
    }
    std::cout << gap << ": " << Resource::count << std::endl;

    t.meter.fast_forward(1ms);
    Zombie<Resource> z = zs[i];
    EXPECT_EQ(z.shared_ptr()->get_ref().value, i);
    EXPECT_FALSE(z.evicted());
    //zs.pop_back();
  }

  //std::cout << "done" << std::endl;

  while (t.reaper.have_soul()) {
    t.reaper.murder();
  }

  return work_done;
}

void SqrtSpaceLinearTime() {
  struct Test {};

  std::vector<double> work;
  //for (int i = 8; i < 20; ++i) {
  {
    int time = 10000;
    int memory = 25;
    auto v = LinearDependencyForwardBackwardTest<Test>(time, memory);
    std::cout << time << ", " << memory << ": " << v << " = " << v / time << "x " << std::endl;
    work.push_back(v);
  }
}

void LogSpaceNLogNTime() {
  struct Test {};

  std::vector<double> work;
  for (int i = 8; i < 25; ++i) {
    int time = pow(2, 0.5 * i);
    int memory = i * 2;
    auto v = LinearDependencyForwardBackwardTest<Test>(time, i * 2);
    std::cout << time << ", " << memory << ": " << v << std::endl;
    work.push_back(v);
  }
}

TEST(ZombieTest, TailCall) {
  using namespace Default;
  Zombie<int> a(1);
  Zombie<int> b = bindZombieTC([&]() {
    return TailCall([](const int& x){ return Zombie<int>(x + 1); }, a);
  });
  EXPECT_EQ(b.get_value(), 2);
  b.evict();
  EXPECT_EQ(b.get_value(), 2);
}

TEST(ZombieTest, Bind) {
  using namespace Default;
  Zombie<int> x(6), y(7);
  Zombie<int> z = bindZombie([](int x, int y) { return Zombie<int>(x * y); }, x, y);
  EXPECT_EQ(z.get_value(), 42);
}

int main() {
  //TailCall();
  SqrtSpaceLinearTime();
  //LogSpaceNLogNTime();
}
