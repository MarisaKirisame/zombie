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
  Zombie<int> x(42);
  EXPECT_EQ(x.get_value(), 42);
}
