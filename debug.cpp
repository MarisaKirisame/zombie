#include "test/common.hpp"
#include "zombie/zombie.hpp"

#define EXPECT_EQ(x, y) assert((x) == (y));
#define EXPECT_TRUE(x) assert(x);
#define EXPECT_FALSE(x) assert(!(x));

IMPORT_ZOMBIE(default_config)

template<typename T, typename U>
struct GetSize<std::pair<T, U>> {
  size_t operator()(const std::pair<T, U>& p) {
    return GetSize<T>()(p.first) + GetSize<U>()(p.second);
  };
};

int main() {
  struct Test {};
  using Resource = Resource<Test>;

  EXPECT_EQ(Resource::count, 0);
  EXPECT_EQ(Resource::destructor_count, 0);
  unsigned int last_destructor_count;

  Zombie<Resource> x(0);
  {
    last_destructor_count = Resource::destructor_count;
    Zombie<Resource> y = bindZombie([](const Resource& x) { return Zombie<Resource>(x.value + 1); }, x);
    assert(Resource::destructor_count == last_destructor_count);
    last_destructor_count = Resource::destructor_count;
    y.force_unique_evict();
    EXPECT_EQ(Resource::destructor_count, last_destructor_count+1);
    last_destructor_count = Resource::destructor_count;
  }
  EXPECT_EQ(Resource::destructor_count, last_destructor_count);
}
