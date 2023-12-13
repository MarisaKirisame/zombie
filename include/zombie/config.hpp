#pragma once

#include <variant>
#include <iostream>

#include "heap/heap.hpp"
#include "tock/tock.hpp"
#include "common.hpp"

using cost_t = int128_t;
using Metric = cost_t(*)(Time cost, Time neighbor_cost, Space size);

struct ZombieConfig {
  TockTreeImpl tree;
  Metric metric;
  // for some varying metric, such as those concerning UF set,
  // the cost in [Trailokya::book] may not be up-to-date.
  // when the actual value is within [1/approx_factor, approx_factor] of the stored value,
  // we ignore the difference
  // [approx_factor] is stored as a rational number as a/b here. it must be greater than 1.
  std::pair<unsigned int, unsigned int> approx_factor;
};

inline cost_t local_metric(Time cost, Time neighbor_cost, Space size) {
  return (static_cast<int128_t>(cost.count()) * (1UL << 63)) / size.count();
}

inline cost_t uf_metric(Time cost, Time neighbor_cost, Space size) {
  return (static_cast<int128_t>(neighbor_cost.count()) * (1UL << 63)) / size.count();
}

constexpr ZombieConfig default_config = ZombieConfig { TockTreeImpl::Tree, &uf_metric, {2, 1} };
