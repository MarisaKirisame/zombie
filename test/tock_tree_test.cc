#include "Zombie/tock.hpp"

#include <gtest/gtest.h>
#include "assert.hpp"

TEST(TockTest, NumericLimit) {
  ASSERT(std::numeric_limits<Tock>::min().tock == std::numeric_limits<decltype(std::declval<Tock>().tock)>::min());
  ASSERT(std::numeric_limits<Tock>::max().tock == std::numeric_limits<decltype(std::declval<Tock>().tock)>::max());
}

TEST(TockTreeTest, ReversedOrder) {
  tock_tree<int> tt;
  tt.put({2,6}, 1);
  tt.put({1,10}, 2);
  tt.check_invariant();
  EXPECT_EQ(tt.get(5), 1);
  EXPECT_EQ(tt.get(6), 2);
}

TEST(LargestValueLeTest, LVTTest) {
  std::map<int, int> m;
  m.insert({1, 1});
  m.insert({5, 5});
  m.insert({9, 9});
  EXPECT_EQ(largest_value_le(m, 0), m.end());
  EXPECT_EQ(largest_value_le(m, 1)->second, 1);
  EXPECT_EQ(largest_value_le(m, 3)->second, 1);
  EXPECT_EQ(largest_value_le(m, 5)->second, 5);
  EXPECT_EQ(largest_value_le(m, 7)->second, 5);
  EXPECT_EQ(largest_value_le(m, 9)->second, 9);
  EXPECT_EQ(largest_value_le(m, 10)->second, 9);
}
