#pragma once

#include <memory>

#define ZOMBIE_KINETIC_VERIFY_INVARIANT

#include "zombie/zombie.hpp"
#include "zombie/heap/heap.hpp"

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
  void operator()(const Element<is_unique>&, const size_t&) { }
};





template<>
struct GetSize<int> {
  size_t operator()(const int&) {
    return sizeof(int);
  }
};



// [test_id] is used to separate different tests
template<typename test_id>
struct Resource {
  static unsigned int count;
  static unsigned int destructor_count;

  int value;

  Resource(int value) : value(value) { ++count; }
  Resource(const Resource&) = delete;
  ~Resource() { --count; ++destructor_count; }
};

template<typename test_id>
unsigned int Resource<test_id>::count = 0;

template<typename test_id>
unsigned int Resource<test_id>::destructor_count = 0;


template<typename test_id>
struct GetSize<Resource<test_id>> {
  size_t operator()(const Resource<test_id>&) {
    return 1 << 16;
  };
};
