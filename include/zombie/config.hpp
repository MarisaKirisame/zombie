#pragma once

#include <variant>

#include "heap/heap.hpp"
#include "tock/tock.hpp"

#include "common.hpp"


using AffMetric = AffFunction(*)(Time, Time, Space);

struct ZombieConfig {
  KineticHeapImpl heap;
  TockTreeImpl tree;
  AffMetric metric;
};



AffFunction default_metric(Time last_accessed, Time cost, Space size) {
  // This is a min heap, so we have to flip the slope.
  // Recently accessed node should have the higest score of 0, so shift is just -last_accessed.
  return AffFunction {
    -((static_cast<int128_t>(cost.count()) * (1UL << 63)) / size.count()),
    - last_accessed.count()
  };
}

constexpr ZombieConfig default_config = ZombieConfig { KineticHeapImpl::Bag, TockTreeImpl::Tree, &default_metric };
