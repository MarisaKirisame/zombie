#pragma once

#include <memory>

#include "zombie/kinetic.hpp"

template<bool is_unique>
struct Element;

template<>
struct Element<false> {
  int i;

  int get() const { return i; }
  bool operator<(const Element<false> &other) const { return get() < other.get(); }
  bool operator==(const Element<false> &other) const { return get() == other.get(); }
};

template<>
struct Element<true> {
  std::unique_ptr<int> ptr;

  template<typename... Args>
  Element(Args&&... args) : ptr(std::make_unique<int>(std::forward<Args>(args)...)) {}

  int get() const { return *ptr; }
  bool operator<(const Element<true> &other) const { return get() < other.get(); }
  bool operator==(const Element<true> &other) const { return get() == other.get(); }
};


template<bool is_unique>
struct NotifyHeapIndexChanged<Element<is_unique>> {
  void operator()(const Element<is_unique>& n, const size_t& idx) { }
};

template<bool is_unique>
struct NotifyHeapElementRemoved<Element<is_unique>> {
  void operator()(const Element<is_unique>& i) { }
};


template<bool is_unique>
struct NotifyIndexChanged {
  void operator()(const Element<is_unique>&, const size_t&) {
  }
};
