#pragma once

#include <variant>

#include "common.hpp"
#include "cache.hpp"
#include "tree.hpp"

enum class TockTreeImpl {
  Tree = 0
};


template<TockTreeImpl impl, typename T, typename NotifyParentChanged>
using TockTree = std::variant_alternative_t<
  (size_t)impl,
  std::variant<
    TockTreeImpls::TockTree<T, TockTreeCaches::HashCache, NotifyParentChanged>
  >
>;
