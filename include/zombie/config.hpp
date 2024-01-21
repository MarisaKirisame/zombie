#pragma once

#include <variant>
#include <iostream>

#include "heap/heap.hpp"
#include "tock/tock.hpp"
#include "common.hpp"

using cost_t = int128_t;
inline std::ostream& operator<<(std::ostream& o, const cost_t& x) {
  if (x == std::numeric_limits<cost_t>::min()) {
    return o << "-170141183460469231731687303715884105728";
  } else if (x < 0) {
    return o << "-" << -x;
  } else if (x < 10) {
    return o << (char)(x + '0');
  } else {
    return o << x / 10 << (char)(x % 10 + '0');
  }
}
using Metric = cost_t(*)(Time cost, Time neighbor_cost, Space size);

struct ZombieConfig {
  // There are two zombie mode,
  // The ANF('default mode') and the cps mode.
  // The cps mode is more efficient but it does not allow bindZombie inside bindZombie.
  // Instead every call must be tailcall.
  bool use_cps;
  Metric metric;
  // for some varying metric, such as those concerning UF set,
  // the cost in [Trailokya::book] may not be up-to-date.
  // when the actual value is within [1/approx_factor, approx_factor] of the stored value,
  // we ignore the difference
  // [approx_factor] is stored as a rational number as a/b here. it must be greater than 1.
  std::pair<unsigned int, unsigned int> approx_factor;
  constexpr ZombieConfig(const bool& use_cps,
               const Metric& metric,
               const std::pair<unsigned int, unsigned int>& approx_factor) :
    use_cps(use_cps),
    metric(metric),
    approx_factor(approx_factor)
  { }
};

inline cost_t local_metric(Time cost, Time neighbor_cost, Space size) {
  return (static_cast<int128_t>(cost.count()) * (1UL << 63)) / size.count();
}

inline cost_t uf_metric(Time cost, Time neighbor_cost, Space size) {
  return (static_cast<int128_t>(neighbor_cost.count()) * (1UL << 63)) / size.count();
}

constexpr ZombieConfig default_config(/*use_cps=*/false, /*metric=*/&uf_metric, /*approx_factor=*/{2, 1});
