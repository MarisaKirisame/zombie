#pragma once

#include <variant>

#include "common.hpp"
#include "cache.hpp"
#include "tree.hpp"

template<typename T, typename NotifyParentChanged>
using TockTree = TockTreeImpls::TockTree<T, TockTreeCaches::SplayCache, NotifyParentChanged>;
