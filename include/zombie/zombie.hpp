#pragma once

#include <memory>
#include <functional>
#include <vector>

#include "common.hpp"
#include "config.hpp"
#include "zombie_types.hpp"
#include "trailokya.hpp"
#include "zombie_impl.hpp"

#define IMPORT_ZOMBIE(cfg)\
    template<typename T>\
    using Zombie = ZombieInternal::Zombie<cfg, T>;\
    using Trailokya = ZombieInternal::Trailokya<cfg>;\
    template<typename F, typename... Args>\
    inline auto bindZombie(F&& f, const Zombie<Args>& ...x) { return ZombieInternal::bindZombie<cfg, F, Args...>(std::move(f), x...); }\
    template<typename F>\
    inline auto bindZombieUnTyped(F&& f, const std::vector<ZombieInternal::EZombie<cfg>>& x) { return ZombieInternal::bindZombieUnTyped<cfg, F>(std::move(f), x); }
