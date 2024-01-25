#pragma once

#include <variant>

#include "../config.hpp"
#include "common.hpp"
#include "cache.hpp"
#include "tree.hpp"

template<const ZombieConfig& cfg, typename T, typename NotifyParentChanged>
using TockTree = TockTreeImpls::TockTree<T, TockTreeCaches::SplayCache, NotifyParentChanged>;
