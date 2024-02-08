#include "common.hpp"
#include "zombie/meter.hpp"

#include <gtest/gtest.h>

TEST(ZombieRawClockTest, Time) {
  ZombieClock& zc = ZombieClock::singleton();
  auto a = zc.time();
  auto b = zc.time();
  EXPECT_TRUE(b >= a);
  zc.fast_forward(1s);
  auto c = zc.time();
  EXPECT_TRUE(c > b);
  EXPECT_TRUE((c - b) >= 1s);
  EXPECT_TRUE((c - b) < 2s);
}

TEST(ZombieClockTest, Time) {
  struct Unit { };
  ZombieMeter zc;

  struct RecurBlocked {
    ZombieMeter& zc;
    void operator()(size_t i) {
      if (i > 0) {
        auto p = zc.measured([&]() {
          zc.fast_forward(1s);
          zc.block([&]() {
            (*this)(i - 1);
          });
          zc.fast_forward(1s);
          return Unit();
        });
        auto t = std::get<1>(p);
        EXPECT_TRUE(t >= 2s);
        EXPECT_TRUE(t < 3s);
      }
    }
  };
  RecurBlocked({zc})(10);

  struct RecurNoBlocked {
    ZombieMeter& zc;
    void operator()(size_t i) {
      if (i > 0) {
        auto p = zc.measured([&]() {
          zc.fast_forward(1s);
          (*this)(i - 1);
          zc.fast_forward(1s);
          return Unit();
        });
        auto t = std::get<1>(p);
        EXPECT_TRUE(t > i * 2s);
        EXPECT_TRUE(t < i * 2s + 1s);
      }
    }
  };
  RecurNoBlocked({zc})(10);
}
