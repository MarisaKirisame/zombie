#include "zombie/heap/segmented_array.hpp"

#include <gtest/gtest.h>

TEST(Segemnted, Basic) {
    SegmentedArray<int> sa;

    EXPECT_EQ(sa.empty(), true);
    EXPECT_EQ(sa.size(), 0);

    for (int i = 0; i < 10; i++) {
        sa.push_back(i);
    }

    EXPECT_EQ(sa.size(), 10);

    sa.pop_back();

    EXPECT_EQ(sa.size(), 9);

    int t = 0;
    for (auto a : sa) {
        EXPECT_EQ(a, t);
        t++;
    }
}