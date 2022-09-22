#pragma once

#include "phantom.hpp"
#include "zombie.hpp"

inline const void* Phantom::void_ptr() const {
  if (!has_value()) {
    auto& n = *World::get_world().record.get_precise_node(created_time).parent;
    dynamic_cast<Computer*>(n.value)->replay(n.range.first);
  }
  ASSERT(has_value());
  return a.void_ptr();
}
