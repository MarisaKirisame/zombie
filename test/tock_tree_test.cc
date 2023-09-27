#include "common.hpp"
#include "zombie/tock/tock.hpp"

#include <gtest/gtest.h>

struct NotifyParentChanged {
  void operator()(const TockTreeData<int>& n, const TockTreeData<int>* p) {
  }
};

TEST(TockTest, NumericLimit) {
  assert(std::numeric_limits<Tock>::min().tock == std::numeric_limits<decltype(std::declval<Tock>().tock)>::min());
  assert(std::numeric_limits<Tock>::max().tock == std::numeric_limits<decltype(std::declval<Tock>().tock)>::max());
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

template<TockTreeImpl impl>
void TockTreeTestReverseOrder() {
  TockTree<impl, int, NotifyParentChanged> tt;
  tt.put({2,6}, 1);
  tt.put({1,10}, 2);
  tt.check_invariant();
  EXPECT_EQ(tt.get_node(5).value, 1);
  EXPECT_EQ(tt.get_node(6).value, 2);
}

TEST(TockTreeTest, ReversedOrder) {
  TockTreeTestReverseOrder<TockTreeImpl::Tree>();
}



template<TockTreeImpl impl>
void TockTreeTestFilterChildren() {
  TockTree<impl, int, NotifyParentChanged> tt;
  tt.put({1, 10}, 1);
  tt.put({2, 3}, 2);
  tt.put({3, 4}, 3);
  tt.put({4, 10}, 4);
  tt.put({5, 6}, 5);
  tt.put({6, 7}, 6);
  tt.check_invariant();

  EXPECT_EQ(tt.get_precise_node(2).value, 2);
  EXPECT_EQ(tt.get_precise_node(3).value, 3);
  EXPECT_EQ(tt.get_precise_node(4).value, 4);
  EXPECT_EQ(tt.get_precise_node(5).value, 5);
  EXPECT_EQ(tt.get_precise_node(6).value, 6);

  EXPECT_FALSE(tt.has_precise(7));
  EXPECT_EQ(tt.get_node(7).value, 4);

  tt.filter_children([](const auto& d) {
    return d.value == 2 || d.value == 3;
  }, 1);

  EXPECT_FALSE(tt.has_precise(2));
  EXPECT_EQ(tt.get_node(2).value, 1);
  EXPECT_FALSE(tt.has_precise(3));
  EXPECT_EQ(tt.get_node(3).value, 1);

  EXPECT_EQ(tt.get_precise_node(4).value, 4);
  EXPECT_EQ(tt.get_precise_node(5).value, 5);
  EXPECT_EQ(tt.get_precise_node(6).value, 6);
}

TEST(TockTreeTest, FilterChildren) {
  TockTreeTestFilterChildren<TockTreeImpl::Tree>();
}
