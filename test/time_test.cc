#include "zombie/time.hpp"

#include <gtest/gtest.h>

using namespace std::chrono_literals;

TEST(ZombieRawClockTest, Time) {
  ZombieRawClock& zc = ZombieRawClock::singleton();
  auto a = zc.time();
  auto b = zc.time();
  EXPECT_GT(b, a);
  zc.fast_forward(1s);
  auto c = zc.time();
  EXPECT_GT(c, b);
  EXPECT_GE((c - b), 1s);
  EXPECT_LT((c - b), 2s);
}

TEST(ZombieClockTest, Time) {
  struct Unit { };
  ZombieClock zc;

  auto time_taken = zc.timed<Unit>([&]() {
    zc.fast_forward(1s);
    auto t = zc.timed<Unit>([&]() {
      zc.fast_forward(1s);
      return Unit();
    }).second;
    EXPECT_GE(t, 1s);
    return Unit();
  }).second;
  EXPECT_GE(time_taken, 1s);
  EXPECT_LT(time_taken, 2s);
}
