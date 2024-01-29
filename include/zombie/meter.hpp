#pragma once

#include <chrono>
#include <utility>
#include <vector>
#include <tuple>
#include <functional>
#include <iostream>
#include <cassert>

#include "common.hpp"

// A slight wrapper above standard clock,
//   providing fast forwarding ability.
// Useful for testing, and for including additional time uncaptured in bindZombie,
//   e.g. Zombie's use in gegl.
// TODO: the right way is to add configurability to Zombie, not via such an adhoc patch.
struct ZombieClock {
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

  static ZombieClock& singleton() {
    static ZombieClock zc;
    return zc;
  }
};

// In zombie, we want the time or bindZombie to not include that of recursive remat.
// This class take care of that.
struct ZombieMeter {
  struct Node {
    ns constructed_time = ZombieClock::singleton().time();
    ns skipping_time = ns(0);

    ns time() {
      return ZombieClock::singleton().time() - skipping_time;
    }
  };

  std::vector<Node> stack { Node() };

  ns time() {
    return stack.back().time();
  }

  ns raw_time() const {
    return ZombieClock::singleton().time();
  }

  void fast_forward(ns n) {
    ZombieClock::singleton().fast_forward(n);
  }

  template<typename F>
  std::pair<decltype(std::declval<F>()()), ns> measured(const F& f) {
    assert(!stack.empty());
    size_t ss = stack.size();
    ns time_before = time();
    auto t = f();
    assert(ss == stack.size());
    ns time_after = time();
    return {std::move(t), time_after - time_before};
  }

  template<typename F>
  decltype(std::declval<F>()()) block(const F& f) {
    return bracket([&]() { stack.push_back(Node()); },
                   f,
                   [&]() {
                     assert(!stack.empty());
                     ns constructed_time = stack.back().constructed_time;
                     ns skipping_time = stack.back().skipping_time;
                     ns current_time = stack.back().time();
                     assert(current_time > constructed_time);
                     ns taken_time = current_time - constructed_time;
                     stack.pop_back();
                     assert(!stack.empty());
                     stack.back().skipping_time += taken_time + skipping_time;
                   });
  }
};
