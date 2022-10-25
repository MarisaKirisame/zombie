#pragma once

#include "base.hpp"
#include "any.hpp"
#include "world.hpp"
/*
struct Phantom : EMonster {
  mutable Any a;
  tock created_time;
  bool evictable() const { return false; }
  void lock() const { }
  void unlock() const { }
  bool has_value() const { return a.has_value(); }
  void evict() const { ASSERT(false); }
  const void* void_ptr() const;
  void fill_any(Any&& filled) const {
    if (!a.has_value()) {
      a = std::move(filled);
    }
  }
  Phantom(const tock& created_time) : created_time(created_time) {
    World::get_world().record.put({created_time, created_time+1}, this);
  }
  ~Phantom() {
    World::get_world().record.get_precise_node(created_time).delete_node();
  }
};
*/
