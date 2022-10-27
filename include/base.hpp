#pragma once

#include "any.hpp"

struct Object {
  virtual ~Object() { }
};

// template<typename T>
struct EMonster : Object {
  // const T* T_ptr() const = 0;
  virtual const void* void_ptr() const = 0;
  // locked values are not evictable
  virtual bool evictable() const = 0;
  virtual bool has_value() const = 0;
  // a value may be evicted multiple time.
  // this serve as a better user-facing api,
  // because a value might be silently evict.
  virtual void evict() const = 0;
  void force_evict() const {
    ASSERT(evictable());
    evict();
  }
  // void fill(T&&) const = 0;
  virtual void fill_any(Any&&) const = 0;
};

