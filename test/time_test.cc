#include "zombie/time.hpp"

#include <gtest/gtest.h>

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

  struct RecurBlocked {
    ZombieClock& zc;
    void operator()(size_t i) {
      if (i > 0) {
        auto t = zc.timed([&]() {
          zc.fast_forward(1s);
          zc.block([&]() {
            (*this)(i - 1);
          });
          zc.fast_forward(1s);
          return Unit();
        }).second;
        EXPECT_GE(t, 2s);
        EXPECT_LT(t, 3s);
      }
    }
  };
  RecurBlocked({zc})(10);

  struct RecurNoBlocked {
    ZombieClock& zc;
    void operator()(size_t i) {
      if (i > 0) {
        auto t = zc.timed([&]() {
          zc.fast_forward(1s);
          (*this)(i - 1);
          zc.fast_forward(1s);
          return Unit();
        }).second;
        EXPECT_GE(t, i * 2s);
        EXPECT_LT(t, i * 2s + 1s);
      }
    }
  };
  RecurNoBlocked({zc})(10);
}
