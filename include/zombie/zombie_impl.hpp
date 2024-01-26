#pragma once

#include "zombie_impl_anf.hpp"
#include "zombie_impl_cps.hpp"

namespace ZombieInternal {

template<const ZombieConfig& cfg>
EZombieNode<cfg>::EZombieNode(Tock created_time)
  : created_time(created_time) { }

template<const ZombieConfig& cfg>
void EZombieNode<cfg>::accessed() const {
  auto parent = get_parent();
  if (parent) {
    parent->accessed();
  }
}

template<const ZombieConfig& cfg>
std::shared_ptr<MicroWave<cfg>> EZombieNode<cfg>::get_parent() const {
  auto ret = parent_cache.lock();
  if (!ret) {
    auto& t = Trailokya<cfg>::get_trailokya();
    auto parent = t.akasha.get_parent(created_time);
    if (!parent || parent->value.index() == TockTreeElemKind::Nothing) {
      parent_cache = std::weak_ptr<MicroWave<cfg>>();
      return nullptr;
    } else {
      ret = std::get<TockTreeElemKind::MicroWave>(parent->value);
      parent_cache = ret;
    }
  }
  return ret;
}

template<const ZombieConfig& cfg, typename T>
template<typename... Args>
ZombieNode<cfg, T>::ZombieNode(Tock created_time, Args&&... args) : EZombieNode<cfg>(created_time), t(std::forward<Args>(args)...) {
  Trailokya<cfg>::get_trailokya().meter.add_space(GetSize<T>()(t));
}

template<const ZombieConfig& cfg>
std::weak_ptr<EZombieNode<cfg>> EZombie<cfg>::ptr() const {
  if (ptr_cache.expired()) {
    Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
    if (t.akasha.has_precise(created_time)) {
      ptr_cache = std::get<TockTreeElemKind::ZombieNode>(t.akasha.get_precise_node(created_time).value);
    }
  }
  return ptr_cache;
}

template<const ZombieConfig& cfg>
cost_t RecomputeLater<cfg>::cost() const {
  return non_null(weak_ptr.lock())->cost();
}

template<const ZombieConfig& cfg>
void RecomputeLater<cfg>::evict() {
  auto& t = Trailokya<cfg>::get_trailokya();
  non_null(weak_ptr.lock())->evict();
  t.akasha.filter_children([](const auto& d) {
    return d.value.index() == TockTreeElemKind::ZombieNode;
  }, created_time);
}

template<const ZombieConfig& cfg>
void EZombie<cfg>::evict() {
  if (evictable()) {
    Trailokya<cfg>::get_trailokya().akasha.remove_precise(created_time);
  }
}

template<const ZombieConfig& cfg>
std::shared_ptr<EZombieNode<cfg>> EZombie<cfg>::shared_ptr() const {
  auto ret = ptr().lock();
  if (ret) {
    return ret;
  } else {
    auto& t = Trailokya<cfg>::get_trailokya();
    if (!t.akasha.has_precise(created_time)) {
      std::shared_ptr<EZombieNode<cfg>> strong;
      typename Trailokya<cfg>::Tardis tardis = t.tardis;
      bracket([&]() { t.tardis = typename Trailokya<cfg>::Tardis { this->created_time, &strong }; },
              [&]() { std::get<TockTreeElemKind::MicroWave>(t.akasha.get_node(created_time).value).replay(); },
              [&]() { t.tardis = tardis; });
      ret = non_null(strong);
    } else {
      auto& n = t.akasha.get_precise_node(created_time);
      ret = std::get<TockTreeElemKind::ZombieNode>(n.value);
    }
    ptr_cache = ret;
    return ret;
  }
}

template<const ZombieConfig& cfg, typename T>
template<typename... Args>
void Zombie<cfg, T>::construct(Args&&... args) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  if (t.tardis.is_partial) {
    assert(t.tardis.forward_to == nullptr || *t.tardis.forward_to);
    EZombie<cfg>::created_time = std::numeric_limits<Tock>::max();
    // why bother increasing the tock? well - the larger this tock is the larger that of capturing microwave.
    // this potentially cause a replay at a more precise partial node.
    t.current_tock++;
  } else {
    EZombie<cfg>::created_time = t.current_tock++;
    if (!t.akasha.has_precise(EZombie<cfg>::created_time)) {
      auto shared = std::make_shared<ZombieNode<cfg, T>>(this->created_time, std::forward<Args>(args)...);
      this->ptr_cache = shared;
      t.akasha.put({this->created_time, this->created_time + 1}, { shared });
    }
    if (t.tardis.forward_at == this->created_time) {
      *t.tardis.forward_to = shared_ptr();
    }
  }
}

template<const ZombieConfig& cfg, typename F, typename... Args>
inline auto bindZombie(F&& f, const Zombie<cfg, Args>& ...x) {
  if constexpr (cfg.use_cps) {
    return CPS::bindZombie<cfg, F, Args...>(std::forward<F>(f), x...);
  }
  else {
    return ANF::bindZombie<cfg, F, Args...>(std::forward<F>(f), x...);
  }
}

template<const ZombieConfig& cfg, typename F>
inline auto bindZombieUnTyped(F&& f, const std::vector<EZombie<cfg>>& x) {
  if constexpr (cfg.use_cps) {
    return CPS::bindZombieUnTyped<cfg, F>(std::forward<F>(f), x);
  }
  else {
    return ANF::bindZombieUnTyped<cfg, F>(std::forward<F>(f), x);
  }
}

// TODO: remove Ret
template<const ZombieConfig& cfg, typename Ret, typename F, typename... Args>
inline auto bindZombieTC(F&& f, const Zombie<cfg, Args>& ...x) {
  if constexpr (cfg.use_cps) {
    return CPS::bindZombieTC<cfg, Ret, F, Args...>(std::forward<F>(f), x...);
  }
  else {
    return ANF::bindZombieTC<cfg, Ret, F, Args...>(std::forward<F>(f), x...);
  }
}

template<const ZombieConfig& cfg, typename F, typename... Args>
inline auto TailCall(F&& f, const Zombie<cfg, Args>& ...x) {
  if constexpr (cfg.use_cps) {
    return CPS::TailCall<cfg, F, Args...>(std::forward<F>(f), x...);
  }
  else {
    return ANF::TailCall<cfg, F, Args...>(std::forward<F>(f), x...);
  }
}

} // end of namespace ZombieInternal
