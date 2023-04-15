#pragma once

#include <memory>

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
