#pragma once

template<typename A, typename B, typename C>
decltype(std::declval<B>()()) bracket(const A& a, const B& b, const C& c) {
  a();
  auto ret = b();
  c();
  return ret;
}
