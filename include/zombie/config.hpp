#pragma once

#include <variant>

#include "heap/heap.hpp"
#include "tock/tock.hpp"

#include "common.hpp"

enum class EvictionPolicy {
  Zombie,
  LeastRecentlyUsed,
};

using AffMetric = AffFunction(*)(Time last_accessed, Time cost, Time neighbor_cost, Space size);

struct ZombieConfig {
  KineticHeapImpl heap;
  TockTreeImpl tree;
  AffMetric metric;
  // for some varying metric, such as those concerning UF set,
  // the aff function in [Trailokya::book] may not be up-to-date.
  // when the actual value is within [1/approx_factor, approx_factor] of the stored value,
  // we ignore the difference
  // [approx_factor] is stored as a rational number here. it must be greater than 1.
  std::pair<unsigned int, unsigned int> approx_factor;

  bool if_count_eviction;
  EvictionPolicy eviction_policy;
};



inline AffFunction local_metric(Time last_accessed, Time cost, Time neighbor_cost, Space size) {
  // This is a min heap, so we have to flip the slope.
  // Recently accessed node should have the higest score of 0, so shift is just -last_accessed.
  return AffFunction {
    -((static_cast<int128_t>(cost.count()) * (1UL << 63)) / size.count()),
    - last_accessed.count()
  };
}


inline AffFunction uf_cost_metric(Time last_accessed, Time cost, Time neighbor_cost, Space size) {
  return AffFunction { neighbor_cost.count(), 0 };
}

inline AffFunction uf_metric(Time last_accessed, Time cost, Time neighbor_cost, Space size) {
  return AffFunction {
    -((static_cast<int128_t>(neighbor_cost.count()) * (1UL << 63)) / size.count()),
    - last_accessed.count()
  };
}

constexpr ZombieConfig default_config = ZombieConfig { 
  KineticHeapImpl::Heap, 
  TockTreeImpl::Tree, 
  &local_metric, 
  {1, 1},
  true,
  EvictionPolicy::Zombie,
 };
