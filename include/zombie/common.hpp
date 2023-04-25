#pragma once

#include <memory>
#include <chrono>
#include <cassert>

template<typename A, typename B, typename C>
decltype(std::declval<B>()()) bracket(const A& a, const B& b, const C& c) {
  a();
  struct Guard {
    const C& c;
    ~Guard() {
      c();
    }
  } g({c});
  return b();
}


template<typename T>
T non_null(T&& x) {
  assert(x);
  return std::forward<T>(x);
}


template <typename T>
bool weak_is_nullptr(std::weak_ptr<T> const& weak) {
  using wt = std::weak_ptr<T>;
  return !weak.owner_before(wt{}) && !wt{}.owner_before(weak);
}

static_assert(sizeof(__int128) == 16);
using int128_t = __int128;
using ns = std::chrono::nanoseconds;
using namespace std::chrono_literals;

// Note that we need to divide space by time, and since we are using fixed int,
//   we have to fence against underflow.
// The idea is to multiply space measurement by 2^64,
//   so space will always be > time (time is stored in 64 bits).
// Note that the result is in 192 bit, so there's a huge likelihood of overflowing.
// To combat this, we are trimming some insiginificant bit in time and space measurement.

constexpr size_t plank_time_in_nanoseconds = 1024;
constexpr size_t plank_space_in_bytes = 16;

struct Time {
  ns time;

  Time() = delete;
  Time(ns time) : time(time) { }

  int64_t count() {
    assert(time.count() >= plank_time_in_nanoseconds);
    return time.count() / plank_time_in_nanoseconds;
  }
};

struct Space {
  size_t bytes;

  Space() = delete;
  // an ad hoc hack because we need test to run.
  // TODO: fix this by having plank constant configurable
  Space(size_t bytes) : bytes(bytes + plank_space_in_bytes) { }

  size_t count() {
    assert(bytes >= plank_space_in_bytes);
    return bytes / plank_space_in_bytes;
  }
};


template<typename T>
struct GetSize; // {
//   Space operator()(const T&);
// };
