#pragma once

#include <memory>
#include "assert.hpp"

struct Any {
  struct AnyNode {
    virtual ~AnyNode() { }
    virtual const void* void_ptr() const = 0;
    virtual void* void_ptr() = 0;
  };
  std::unique_ptr<AnyNode> ptr;
  bool has_value() const {
    return static_cast<bool>(ptr);
  }
  const void* void_ptr() const {
    return ptr->void_ptr();
  }
  void* void_ptr() {
    ASSERT(has_value());
    return ptr->void_ptr();
  }
  template<typename T>
  struct AnyNodeImpl : AnyNode {
    std::decay_t<T> t;
    AnyNodeImpl(T&& t) : t(std::forward<T>(t)) { }
    const void* void_ptr() const {
      return &t;
    }
    void* void_ptr() {
      return &t;
    }
  };
  template<typename T>
  Any(T&& t) : ptr(std::make_unique<AnyNodeImpl<T>>(std::forward<T>(t))) { }
  Any() = default;
  Any(const Any& t) = delete;
  Any(Any&& t) : ptr(std::move(t.ptr)) { }
  Any& operator=(Any&& t) {
    ptr = std::move(t.ptr);
    return *this;
  }
};
