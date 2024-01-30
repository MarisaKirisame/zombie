#include "test/common.hpp"
#include "zombie/zombie.hpp"

#define EXPECT_EQ(x, y) assert((x) == (y));
#define EXPECT_LT(x, y) assert((x) < (y));
#define EXPECT_TRUE(x) assert(x);
#define EXPECT_FALSE(x) assert(!(x));

constexpr ZombieConfig uf_cfg(/*metric=*/&uf_metric, /*approx_factor=*/{1, 1});

namespace UF {
  IMPORT_ZOMBIE(uf_cfg)
}

// access a list of zombies with linear dependency, first from beginning to end,
// then from end to beginning.
// - total_size: total number of zombies
// - memory_limit: number of zombies alive
// - return value: the total count of computations, including rematerialization
template<typename test_id>
unsigned int LinearDependencyForwardBackwardTest(unsigned int total_size, unsigned int memory_limit) {
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


  unsigned int work_done = 0;
  zs.push_back(bindZombie([&]() {
    allocate_memory();
    t.meter.fast_forward(100s);
    ++work_done;
    return Zombie<Resource>(0);
  }));

  // forward
  for (int i = 1; i < total_size; ++i) {
    zs.push_back(bindZombie([&](const Resource& x) {
      allocate_memory();
      t.meter.fast_forward(100s);
      ++work_done;
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

  while (t.reaper.have_soul()) {
    t.reaper.murder();
  }

  return work_done;
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

void SqrtSpaceLinearTime() {
  struct Test {};

  std::vector<double> work;
  for (int i = 8; i < 20; ++i) {
    work.push_back(LinearDependencyForwardBackwardTest<Test>(i * i, 2 * i));
  }

  double r2 = r_square(work, [](unsigned int x) { return 8 + x; });
  EXPECT_LT(0.9, r2);
  EXPECT_LT(r2, 1.0);
}

void LogSpaceNLogNTime() {
  struct Test {};

  std::vector<double> work;
  for (int i = 8; i < 20; ++i) {
    work.push_back(LinearDependencyForwardBackwardTest<Test>(pow(2, 0.5 * i), 2 * i));
  }

  double r2 = r_square(work, [](unsigned int x) {
    double size = pow(2, 0.5 * (8 + x));
    return size * log(size);
  });
  EXPECT_LT(0.9, r2);
  EXPECT_LT(r2, 1.0);
}

int main() {
  SqrtSpaceLinearTime();
  LogSpaceNLogNTime();
}
