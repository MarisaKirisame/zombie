#pragma once

#include <chrono>
#include <utility>
#include <vector>
#include <functional>

// todo: look into code, and remove all RAII replacable by this
template<typename T>
T bracket(const std::function<void()>& open, const std::function<T()>& f, const std::function<void()>& close) {
  struct Guard {
    const std::function<void()>& close;
    Guard(const std::function<void()>& close) : close(close) { }
    ~Guard() {
      close();
    }
  } guard(close);
  open();
  return f();
}

using ns = std::chrono::nanoseconds;

// A slight wrapper above standard clock,
//   providing fast forwarding ability.
// Useful for testing, and for including additional time uncaptured in bindZombie,
//   e.g. Zombie's use in gegl.
// todo: the right way is to add configurability to Zombie, not via such an adhoc patch.
struct ZombieRawClock {
  using time_t = decltype(std::chrono::steady_clock::now());

  time_t begin_time = std::chrono::steady_clock::now();
  // Note that the clock will overflow after only 585 years.

  ns forwarded = ns(0);

  ns time() const {
    return ns(std::chrono::steady_clock::now() - begin_time) + forwarded;
  }

  void fast_forward(ns n) {
    forwarded += n;
  }

  static ZombieRawClock& singleton() {
    static ZombieRawClock zc;
    return zc;
  }
};

// In zombie, we want the time or bindZombie to not include that of recursive bindZombie.
// This class take care of that
struct ZombieClock {
  struct Node {
    ns constructed_time = ZombieRawClock::singleton().time();
    ns skipping_time = ns(0);
  };

  std::vector<Node> stack;

  template<typename T>
  std::pair<T, ns> timed(const std::function<T()>& f) {
    ns skipping_time;
    T t = bracket([&](){
                    stack.push_back(Node());
                  },
                  f,
                  [&](){
                    skipping_time = stack.back().skipping_time;
                    stack.pop_back();
                  });
    return {t, skipping_time};
  }
};
