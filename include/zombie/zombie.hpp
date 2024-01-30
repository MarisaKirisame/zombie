#pragma once

#include <memory>
#include <functional>
#include <vector>

#include "common.hpp"
#include "config.hpp"
#include "zombie_types.hpp"
#include "trailokya.hpp"
#include "zombie_impl.hpp"

#define IMPORT_ZOMBIE(cfg)                                                                         \
  template<typename T>                                                                             \
  using Zombie = ZombieInternal::Zombie<cfg, T>;                                                   \
  using EZombie = ZombieInternal::EZombie<cfg>;                                                    \
  template<typename T>                                                                             \
  using TCZombie = ZombieInternal::TCZombie<cfg, T>;                                               \
  using Trailokya = ZombieInternal::Trailokya<cfg>;                                                \
  template<typename F, typename... Args>                                                           \
  inline auto bindZombie(F&& f, const Zombie<Args>& ...x) {                                        \
    return ZombieInternal::bindZombie<cfg, F, Args...>(std::forward<F>(f), x...);                  \
  }                                                                                                \
  template<typename F, typename... Args>                                             \
  inline auto bindZombieTC(F&& f, const Zombie<Args>& ...x) {                                      \
    return ZombieInternal::bindZombieTC<cfg, F, Args...>(std::forward<F>(f), x...);           \
  }                                                                                                \
  template<typename F, typename... Args>                                                           \
  inline auto TailCall(F&& f, const Zombie<Args>& ...x) {                                          \
    return ZombieInternal::TailCall<cfg, F, Args...>(std::forward<F>(f), x...);                    \
  }                                                                                                \
  ;
