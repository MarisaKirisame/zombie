#pragma once

#include <variant>

#include "bag.hpp"
#include "kinetic.hpp"
#include "tock.hpp"

enum class BookDS {
  Bag = 0,
  KineticHeap,
  KineticHanger
};

enum class TockTreeDS {
  PlainTree = 0
};

using AffMetric = AffFunction(*)(Time, Time, Space);

struct ZombieConfig {
  BookDS     book;
  TockTreeDS tree;
  AffMetric  metric;
};



template<ZombieConfig const &cfg, typename T, typename NotifyIndexChanged>
using Book = std::variant_alternative_t<
  (size_t)cfg.book,
  std::variant<
    Bag<T, NotifyIndexChanged>,
    KineticMinHeap<T, false, NotifyIndexChanged>,
    KineticMinHeap<T, true , NotifyIndexChanged>
  >
>;


template<ZombieConfig const &cfg, typename T, typename NotifyParentChanged>
using TockTree = std::variant_alternative_t<
  (size_t)cfg.tree,
  std::variant<
    tock_tree<T, NotifyParentChanged>
  >
>;




AffFunction default_metric(Time last_accessed, Time cost, Space size) {
  // This is a min heap, so we have to flip the slope.
  // Recently accessed node should have the higest score of 0, so shift is just -last_accessed.
  return AffFunction {
    -((static_cast<int128_t>(cost.count()) * (1UL << 63)) / size.count()),
    - last_accessed.count()
  };
}

constexpr ZombieConfig default_config = ZombieConfig { BookDS::Bag, TockTreeDS::PlainTree, &default_metric };
