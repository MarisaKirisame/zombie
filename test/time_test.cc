#include "zombie/time.hpp"

#include <gtest/gtest.h>

using namespace std::chrono_literals;

TEST(TimeTest, Time) {
  ZombieRawClock& zc = ZombieRawClock::singleton();
  auto a = zc.time();
  auto b = zc.time();
  EXPECT_GT(a, b);
  zc.fast_forward(1s);
  auto c = zc.time();
  EXPECT_GT(c, b);
  EXPECT_GE((c - b), 1s);
  EXPECT_LT((c - b), 2s);
}

