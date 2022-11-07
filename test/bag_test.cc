#include "Zombie/bag.hpp"

#include <gtest/gtest.h>
#include "assert.hpp"

TEST(BagTest, Bag) {
  Bag<int> b({0, 1, 2, 3, 4});
  EXPECT_EQ(b.size(), 5);
  EXPECT_EQ(b, Bag<int>({4, 3, 2, 1, 0}));
  b.remove(2);
  EXPECT_EQ(b.size(), 4);
  EXPECT_NE(b, Bag<int>({4, 3, 2, 1, 0}));
  EXPECT_EQ(b, Bag<int>({4, 3, 1, 0}));
}
