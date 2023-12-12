#pragma once

#include <variant>

#include "tock/tock.hpp"
#include "common.hpp"

struct ZombieConfig {
  KineticHeapImpl heap;
  TockTreeImpl tree;
  // for some varying metric, such as those concerning UF set,
  // the aff function in [Trailokya::book] may not be up-to-date.
  // when the actual value is within [1/approx_factor, approx_factor] of the stored value,
  // we ignore the difference
  // [approx_factor] is stored as a rational number here. it must be greater than 1.
  std::pair<unsigned int, unsigned int> approx_factor;
};

constexpr ZombieConfig default_config = ZombieConfig { KineticHeapImpl::Bag, TockTreeImpl::Tree, {1, 1} };
