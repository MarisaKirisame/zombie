#include "zombie/zombie.hpp"
#include "zombie/kinetic.hpp"
#include "zombie/time.hpp"

using namespace std::chrono_literals;

int main() {
  struct Unit { };
  ZombieClock zc;

  auto time_taken = zc.timed([&]() {
    zc.fast_forward(1s);
    auto t = zc.timed([&]() {
      zc.fast_forward(1s);
      return Unit();
    }).second;
    assert(t >= 1s);
    return Unit();
  }).second;
  assert(time_taken >= 1s);
  assert(time_taken < 2s);
}
