#pragma once

#include <iostream>
#include <type_traits>
#include <set>

#include "zombie/zombie.hpp"
#include "zombie/common.hpp"
#include "zombie/trailokya.hpp"

namespace ZombieInternal {

template<const ZombieConfig &cfg, typename T>
TCZombie<cfg, T>::TCZombie(std::function<Trampoline::Output<EZombie<cfg>>()>&& f) : o(std::make_shared<Trampoline::TCNode<EZombie<cfg>>>(std::move(f))) { }

template<const ZombieConfig &cfg, typename T>
TCZombie<cfg, T>::TCZombie(const Zombie<cfg, T>& z) : o(std::make_shared<Trampoline::ReturnNode<EZombie<cfg>>>(z)) { }

template<const ZombieConfig &cfg, typename T>
TCZombie<cfg, T>::TCZombie(Tock&& t) : o(std::make_shared<Trampoline::ReturnNode<EZombie<cfg>>>(EZombie<cfg>(std::move(t)))) { }

template<const ZombieConfig& cfg>
EZombieNode<cfg>::EZombieNode(Tock created_time)
  : created_time(created_time) { }

template<const ZombieConfig& cfg>
void EZombieNode<cfg>::accessed() const {
  auto context = get_context();
  if (context) {
    context->accessed();
  }
}

inline size_t tock_to_index(const Tock& t, const Tock& context_created_time) {
  assert(t > context_created_time);
  return t.tock - context_created_time.tock - 1;
}

template<const ZombieConfig& cfg>
FullContextNode<cfg>::FullContextNode(std::vector<std::shared_ptr<EZombieNode<cfg>>>&& ez,
                                      const size_t& sp,
                                      const Time& time_taken,
                                      std::function<Trampoline::Output<EZombie<cfg>>(const std::vector<const void*>& in)>&& f,
                                      std::vector<EZombie<cfg>>&& inputs,
                                      const Tock& start_time) :
  ContextNode<cfg>(std::move(ez), sp),
  time_taken(time_taken),
  f(std::move(f)),
  inputs(std::move(inputs)),
  start_time(start_time),
  last_accessed(Trailokya<cfg>::get_trailokya().meter.raw_time()) { }

template<const ZombieConfig& cfg>
void FullContextNode<cfg>::accessed() {
  last_accessed = Trailokya<cfg>::get_trailokya().meter.raw_time();
  if (pool_index != -1) {
    assert(pool_index >= 0);
    Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
    t.book.touch(pool_index);
  }
}

template<const ZombieConfig& cfg>
void FullContextNode<cfg>::evict() {
  this->ez.clear();
  assert(pool_index >= 0);
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  t.book.heap.remove(pool_index);
  pool_index = -1;
}

template<const ZombieConfig& cfg>
void FullContextNode<cfg>::replay() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  Tock tock = t.current_tock;
  Record<cfg> record = t.record;
  bracket(
    [&]() {
      t.current_tock = start_time;
      t.record = std::make_shared<HeadRecordNode<cfg>>(std::move(f), std::move(inputs));
      // be careful! this code will delete this.
      t.akasha.remove_precise(start_time);
    },
    [&]() {
      t.record->play();
    },
    [&]() {
      t.current_tock = tock;
      t.record->complete();
      t.record = record;
    });
}

template<const ZombieConfig& cfg>
std::shared_ptr<ContextNode<cfg>> EZombieNode<cfg>::get_context() const {
  auto ret = context_cache.lock();
  if (!ret) {
    auto& t = Trailokya<cfg>::get_trailokya();
    auto* node = t.akasha.find_le_node(created_time);
    if (node != nullptr && tock_to_index(created_time, node->k) < node->v->ez.size()) {
      context_cache = node->v;
      ret = node->v;
    }
  }
  return ret;
}

template<const ZombieConfig& cfg, typename T>
template<typename... Args>
ZombieNode<cfg, T>::ZombieNode(Tock created_time, Args&&... args) : EZombieNode<cfg>(created_time), t(std::forward<Args>(args)...) { }

template<const ZombieConfig& cfg>
std::weak_ptr<EZombieNode<cfg>> EZombie<cfg>::ptr() const {
  if (ptr_cache.expired()) {
    Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
    auto* node = t.akasha.find_le_node(created_time);
    if (node != nullptr) {
      size_t idx = tock_to_index(created_time, node->k);
      if (idx < node->v->ez.size()) {
        ptr_cache = node->v->ez[idx];
      }
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
  non_null(weak_ptr.lock())->evict();
}

template<const ZombieConfig& cfg>
void EZombie<cfg>::evict() {
  if (evictable()) {
    this->ptr().lock()->get_context()->evict();
  }
}

template<const ZombieConfig& cfg>
std::shared_ptr<EZombieNode<cfg>> EZombie<cfg>::shared_ptr() const {
  auto ret = ptr().lock();
  if (ret) {
    return ret;
  } else {
    auto& t = Trailokya<cfg>::get_trailokya();
    std::shared_ptr<EZombieNode<cfg>> strong;
    Replay replay = t.replay;
    bracket([&]() { t.replay = Replay { created_time, &strong }; },
            [&]() { t.meter.block([&](){ (*(t.akasha.find_le(created_time)))->replay(); }); },
            [&]() { t.replay = replay; });
    ret = non_null(strong);
    ptr_cache = ret;
    return ret;
  }
}

template<const ZombieConfig& cfg, typename T>
template<typename... Args>
void Zombie<cfg, T>::construct(Args&&... args) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  this->created_time = t.current_tock++;
  if (this->created_time <= t.replay.forward_at) {
    auto shared = std::make_shared<ZombieNode<cfg, T>>(this->created_time, std::forward<Args>(args)...);
    this->ptr_cache = shared;
    assert(tock_to_index(this->created_time, t.record->t) ==  + t.record->ez.size());
    t.record->ez.push_back(shared);
    t.record->space_taken += GetSize<T>()(shared->t);
    if (this->created_time == t.replay.forward_at) {
      *t.replay.forward_to = shared;
    }
  }
}

template<const ZombieConfig& cfg>
MicroWave<cfg>::MicroWave(std::function<Trampoline::Output<Tock>(const std::vector<const void*>& in)>&& f,
                          const std::vector<Tock>& inputs,
                          const Tock& output,
                          const Space& space,
                          const Time& time_taken)
  : f(std::move(f)),
    inputs(inputs),
    output(output),
    space_taken(space),
    time_taken(time_taken) {
  assert(false);
  /*auto& t = Trailokya<cfg>::get_trailokya();
  for (const Tock& in : inputs) {
    auto parent = t.akasha.get_parent(in)->value;
    if (parent.index() == TockTreeElemKind::MicroWave) {
      std::get<TockTreeElemKind::MicroWave>(parent)->used_by.push_back(start_time);
    }
    }*/
}

template<const ZombieConfig& cfg>
cost_t MicroWave<cfg>::cost() const {
  assert(false);
  return cfg.metric(time_taken, cost_of_set(), space_taken);
}

template<const ZombieConfig& cfg>
Tock MicroWave<cfg>::root_of_set() const {
  assert(false);
  return info_of_set().first;
}

template<const ZombieConfig& cfg>
Time MicroWave<cfg>::cost_of_set() const {
  assert(false);
  // already evicted
  if (evicted) {
    return info_of_set().second;
  } else {
    // [*this] is not evicted. calculate the cost of UF class after we evict it
    auto& t = Trailokya<cfg>::get_trailokya();
    std::set<Tock> roots;
    Time cost = time_taken;

    auto add_neighbor = [&](const Tock& tock) {
      auto m = t.get_microwave(tock);
      if (m && m->evicted) {
        auto root = m->info_of_set();
        if (roots.find(root.first) == roots.end()) {
          roots.insert(root.first);
          cost = cost + root.second;
        }
      }
    };
    for (const Tock& in : inputs) {
      add_neighbor(in);
    }
    for (const Tock& out : used_by) {
      add_neighbor(out);
    }
    return cost;
  }
}

template<const ZombieConfig& cfg>
std::pair<Tock, Time> MicroWave<cfg>::info_of_set() const {
  assert(false);
  /*
  if (_set_parent == start_time) {
    return { start_time, _set_cost };
  } else {
    auto& t = Trailokya<cfg>::get_trailokya();
    auto parent = std::get<TockTreeElemKind::MicroWave>(t.akasha.get_node(_set_parent).value);
    auto root_info = parent->info_of_set();
    _set_parent = root_info.first;

    return root_info;
    }*/
}

template<const ZombieConfig& cfg>
void MicroWave<cfg>::merge_with(Tock other_tock) {
  assert(false);
  /*
  auto& t = Trailokya<cfg>::get_trailokya();

  auto m = t.get_microwave(other_tock);

  if (! m || ! m->evicted)
    return;

  auto root_info1 = info_of_set();
  auto root_info2 = m->info_of_set();

  if (root_info1.first == root_info2.first)
    return;

  auto root1 = std::get<TockTreeElemKind::MicroWave>(t.akasha.get_node(root_info1.first).value);
  auto root2 = std::get<TockTreeElemKind::MicroWave>(t.akasha.get_node(root_info2.first).value);
  root1->_set_parent = root_info2.first;
  root2->_set_cost = root1->_set_cost + root2->_set_cost;*/
}

template<const ZombieConfig& cfg>
void RootRecordNode<cfg>::suspend() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  assert(this->t < t.replay.forward_at);
  t.akasha.insert(this->t, std::make_shared<RootContextNode<cfg>>(std::move(this->ez), this->space_taken));
}

template<const ZombieConfig& cfg>
void RootRecordNode<cfg>::complete() {
  assert(false);
}

template<const ZombieConfig& cfg>
Record<cfg> RootRecordNode<cfg>::resume() {
  return std::make_shared<RootRecordNode<cfg>>();
}

template<const ZombieConfig& cfg>
HeadRecordNode<cfg>::HeadRecordNode(std::function<Trampoline::Output<EZombie<cfg>>(const std::vector<const void*>& in)>&& f, std::vector<EZombie<cfg>>&& inputs) :
  f(std::move(f)),
  inputs(std::move(inputs)),
  start_time(Trailokya<cfg>::get_trailokya().meter.time()) { }

template<const ZombieConfig& cfg>
void HeadRecordNode<cfg>::suspend() {
  assert(false);
}

template<const ZombieConfig& cfg>
void HeadRecordNode<cfg>::complete() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  assert(this->t != t.replay.forward_at);
  if (this->t < t.replay.forward_at) {
    auto fc = std::make_shared<FullContextNode<cfg>>(std::move(this->ez),
                                                     this->space_taken,
                                                     Time(Trailokya<cfg>::get_trailokya().meter.time()) - start_time,
                                                     std::move(f),
                                                     std::move(inputs),
                                                     this->t);
    t.book.push(std::make_unique<RecomputeLater<cfg>>(fc), fc->cost());
    t.akasha.insert(this->t, fc);
  }
}

template<const ZombieConfig& cfg>
Record<cfg> HeadRecordNode<cfg>::resume() {
  assert(false);
}

template<const ZombieConfig& cfg>
Tock tick() {
  return Trailokya<cfg>::get_trailokya().current_tock++;
}

template<const ZombieConfig& cfg>
Trampoline::Output<EZombie<cfg>> HeadRecordNode<cfg>::play() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  std::vector<const void*> in;
  for (const EZombie<cfg>& input : inputs) {
    in.push_back(input.shared_ptr()->get_ptr());
  }
  return f(in);
}

template<const ZombieConfig& cfg, typename F, typename... Arg>
auto bindZombie(F&& f, const Zombie<cfg, Arg>& ...x) {
  using ret_type = decltype(f(std::declval<Arg>()...));
  static_assert(IsZombie<ret_type>::value, "should be zombie");
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  assert(t.current_tock != t.replay.forward_at);
  if (t.current_tock < t.replay.forward_at) {
    std::function<Trampoline::Output<EZombie<cfg>>(const std::vector<const void*>&)> func =
      [f = std::forward<F>(f)](const std::vector<const void*> in) {
        auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
        std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
        return std::make_shared<Trampoline::ReturnNode<EZombie<cfg>>>(EZombie<cfg>(std::apply([&](const Arg*... arg) { return f(*arg...); }, args)));
      };
    std::vector<EZombie<cfg>> in = {x...};

    t.record->suspend();
    Record<cfg> saved = t.record;

    t.record = std::make_shared<HeadRecordNode<cfg>>(std::move(func), std::move(in));
    Trampoline::Output<EZombie<cfg>> o = t.record->play();

    t.record->complete();
    t.record = saved->resume();

    return ret_type(dynamic_cast<Trampoline::ReturnNode<EZombie<cfg>>*>(o.get())->t);
  } else {
    return ret_type(std::numeric_limits<Tock>::max());
  }
}

template<const ZombieConfig& cfg, typename F, typename... Arg>
auto TailCall(F&& f, const Zombie<cfg, Arg>& ...x) {
  using result_type = decltype(f(std::declval<Arg>()...));
  static_assert(IsTCZombie<result_type>::value, "should be TCZombie");

  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  assert(t.current_tock != t.replay.forward_at);
  if (t.current_tock < t.replay.forward_at) {
    std::function<Trampoline::Output<EZombie<cfg>>()> o = [f = std::forward<F>(f), x...]() {
      std::function<Trampoline::Output<EZombie<cfg>>(const std::vector<const void*>&)> func =
        [f = std::move(f)](const std::vector<const void*> in) {
          auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
          std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
          return std::apply([&](const Arg*... arg) { return f(*arg...); }, args).o;
        };
      Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
      t.record = std::make_shared<HeadRecordNode<cfg>>(std::move(func), std::vector<EZombie<cfg>>{x...});
      auto ret = t.record->play();
      t.record->complete();
      return ret;
    };
    return result_type(std::move(o));
  } else {
    return result_type(std::numeric_limits<Tock>::max());
  }
}

template<const ZombieConfig& cfg, typename F, typename... Arg>
auto bindZombieTC(F&& f, const Zombie<cfg, Arg>& ...x) {
  using result_type = decltype(f(std::declval<Arg>()...));
  static_assert(IsTCZombie<result_type>::value, "result must be be TCZombie");

  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  t.record->suspend();
  Record<cfg> saved = t.record;

  auto trampoline = TailCall(std::forward<F>(f), x...).o;

  while(!trampoline->is_return()) {
    trampoline = trampoline->from_tailcall()();
  }

  t.record = saved->resume();

  return Zombie<cfg, typename TCZombieInner<result_type>::type_t>(trampoline->from_return());
}

} // end of namespace ZombieInternal
