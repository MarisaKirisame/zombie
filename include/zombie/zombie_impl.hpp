#pragma once

#include <iostream>
#include <type_traits>
#include <set>

#include "zombie/zombie.hpp"
#include "zombie/common.hpp"
#include "zombie/trailokya.hpp"

namespace ZombieInternal {

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
HeadContextNode<cfg>::HeadContextNode(std::vector<std::shared_ptr<EZombieNode<cfg>>>&& ez) :
  ContextNode<cfg>(std::move(ez)), last_accessed(Trailokya<cfg>::get_trailokya().meter.raw_time()) { }

template<const ZombieConfig& cfg>
void HeadContextNode<cfg>::accessed() {
  last_accessed = Trailokya<cfg>::get_trailokya().meter.raw_time();
  /* if (pool_index != -1) {
    assert(pool_index >= 0);
    t.book.touch(pool_index);
  }*/
}

template<const ZombieConfig& cfg>
std::shared_ptr<ContextNode<cfg>> EZombieNode<cfg>::get_context() const {
  auto ret = context_cache.lock();
  if (!ret) {
    auto& t = Trailokya<cfg>::get_trailokya();
    auto* node = t.akasha.find_lt_node(created_time);
    if (node != nullptr) {
      context_cache = node->v;
      ret = node->v;
      assert(tock_to_index(created_time, node->k) < node->v->ez.size());
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
    auto* node = t.akasha.find_lt_node(created_time);
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
  assert(false);
  return non_null(weak_ptr.lock())->cost();
}

template<const ZombieConfig& cfg>
void RecomputeLater<cfg>::evict() {
  assert(false);
  /*auto& t = Trailokya<cfg>::get_trailokya();
  non_null(weak_ptr.lock())->evict();
  t.akasha.filter_children([](const auto& d) {
    return d.value.index() == TockTreeElemKind::ZombieNode;
    }, created_time);*/
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
    std::cout << created_time << std::endl;
    assert(false);
    /*
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
    return ret;*/
  }
}

template<const ZombieConfig& cfg, typename T>
template<typename... Args>
void Zombie<cfg, T>::construct(Args&&... args) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  this->created_time = t.current_tock++;
  auto shared = std::make_shared<ZombieNode<cfg, T>>(this->created_time, std::forward<Args>(args)...);
  this->ptr_cache = shared;
  assert(tock_to_index(this->created_time, t.record->t) ==  + t.record->ez.size());
  t.record->ez.push_back(shared);
  t.record->record_space(GetSize<T>()(shared->t));
  if (t.replay.forward_at == this->created_time) {
    *t.replay.forward_to = shared;
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
void MicroWave<cfg>::replay() {
  assert(false);
  /*Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  Tock tock = t.current_tock;
  t.meter.block([&]() {
    bracket([&]() { t.current_tock = start_time; },
            [&]() { ++t.current_tock; play(f, inputs); },
            [&]() { t.current_tock = tock; });
  });

  evicted = false;

  if (_set_parent != start_time) {
    Tock root = root_of_set();
    auto& t = Trailokya<cfg>::get_trailokya();
    auto root_m = std::get<TockTreeElemKind::MicroWave>(t.akasha.get_node(root).value);
    root_m->_set_cost = root_m->_set_cost - time_taken;
    _set_parent = start_time;
    }*/
}

template<const ZombieConfig& cfg>
cost_t MicroWave<cfg>::cost() const {
  assert(false);
  return cfg.metric(time_taken, cost_of_set(), space_taken);
}

template<const ZombieConfig& cfg>
void MicroWave<cfg>::evict() {
  assert(false);
  for (const Tock& t : inputs) {
    merge_with(t);
  }
  for (const Tock& t : used_by) {
    merge_with(t);
  }
  evicted = true;
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
MicroWavePtr<cfg>::MicroWavePtr(std::function<Trampoline::Output<Tock>(const std::vector<const void*>& in)>&& f,
                                const std::vector<Tock>& inputs,
                                const Tock& output,
                                const Space& space,
                                const Time& time_taken) :
  std::shared_ptr<MicroWave<cfg>>(std::make_shared<MicroWave<cfg>>(std::move(f), inputs, output, space, time_taken)) {
  assert(false);
  /*auto& t = Trailokya<cfg>::get_trailokya();
    t.book.push(std::make_unique<RecomputeLater<cfg>>(start_time, *this), (*this)->cost());*/
}

template<const ZombieConfig& cfg>
void MicroWavePtr<cfg>::replay() {
  assert(false);
  (*this)->replay();

  auto& t = Trailokya<cfg>::get_trailokya();
  t.book.push(std::make_unique<RecomputeLater<cfg>>((*this)->start_time, *this), (*this)->cost());
}

template<const ZombieConfig& cfg>
void RootRecordNode<cfg>::finish() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  t.akasha.insert(this->t, std::make_shared<RootContextNode<cfg>>(std::move(this->ez)));
}

template<const ZombieConfig& cfg>
Record<cfg> RootRecordNode<cfg>::resume() {
  return std::make_shared<RootRecordNode<cfg>>();
}

template<const ZombieConfig& cfg>
HeadRecordNode<cfg>::HeadRecordNode(std::function<Trampoline::Output<EZombie<cfg>>(const std::vector<const void*>& in)>&& f, std::vector<EZombie<cfg>>&& inputs) :
  f(std::move(f)),
  inputs(std::move(inputs)),
  space_taken(0),
  start_time(Trailokya<cfg>::get_trailokya().meter.time()) { }

template<const ZombieConfig& cfg>
void HeadRecordNode<cfg>::finish() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  t.akasha.insert(this->t, std::make_shared<HeadContextNode<cfg>>(std::move(this->ez)));
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
  std::function<Trampoline::Output<EZombie<cfg>>(const std::vector<const void*>&)> func =
    [f = std::forward<F>(f)](const std::vector<const void*> in) {
      auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
      std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
      return Result(std::apply([&](const Arg*... arg) { return f(*arg...); }, args)).o;
    };
  std::vector<EZombie<cfg>> in = {x...};

  t.record->finish();
  Record<cfg> saved = t.record;

  t.record = std::make_shared<HeadRecordNode<cfg>>(std::move(func), std::move(in));
  Trampoline::Output<EZombie<cfg>> o = t.record->play();

  t.record->finish();
  t.record = saved->resume();

  return ret_type(dynamic_cast<Trampoline::ReturnNode<EZombie<cfg>>*>(o.get())->t);
}

template<const ZombieConfig& cfg, typename F, typename... Arg>
auto TailCall(F&& f, const Zombie<cfg, Arg>& ...x) {
  assert(false);
  using ret_type = decltype(f(std::declval<Arg>()...));
  static_assert(IsTCZombie<ret_type>::value, "should be TCZombie");
  return *static_cast<ret_type*>(nullptr);
  /*
    Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  assert(t.current_tock != t.tardis.forward_at);
  if (t.current_tock > t.tardis.forward_at) {
    t.current_tock++;
    return ret_type { std::make_shared<Trampoline::ReturnNode<Tock>>(std::numeric_limits<Tock>::max()) };
  } else {
    std::function<Trampoline::Output<Tock>()> o = [f = std::forward<F>(f), x...]() {
      std::function<Trampoline::Output<Tock>(const std::vector<const void*>&)> func =
        [f = std::move(f)](const std::vector<const void*> in) {
          auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
          std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
          return std::apply([&](const Arg*... arg) { return f(*arg...); }, args).o;
        };
      return bindZombieRaw<cfg>(std::move(func), std::move(in));
    };
    return ret_type { std::make_shared<Trampoline::TCNode<Tock>>(o) };
    }*/
}

template<const ZombieConfig& cfg, typename Ret, typename F, typename... Arg>
Zombie<cfg, Ret> bindZombieTC(F&& f, const Zombie<cfg, Arg>& ...x) {
  assert(false);
  /*
  static_assert(std::is_same<decltype(f(std::declval<Arg>()...)), TCZombie<Ret>>::value, "result must be TCZombie");
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  Trampoline::Output<Tock> o = TailCall(std::forward<F>(f), x...).o;
  while(dynamic_cast<Trampoline::ReturnNode<Tock>*>(o.get()) == nullptr) {
    o = dynamic_cast<Trampoline::TCNode<Tock>*>(o.get())->func();
  }
  Tock output = dynamic_cast<Trampoline::ReturnNode<Tock>*>(o.get())->t;
  // Why do we only fix to complete and not to partial?
  // TailCall mean "we are still actively working on it."
  // It will eventually get fixed once that return, so there is no need to touch it.
  // Another way of thinking about it is, changing it to Partial does not have any benefit.
    auto node = t.akasha.visit_node(t.current_tock-1);
    while (node) {
      if (node->data.value.index() == TockTreeElemKind::MicroWave) {
        std::shared_ptr<MicroWave<cfg>> mv = std::get<TockTreeElemKind::MicroWave>(node->data.value);
        if (mv->state == MWState::TailCall()) {
          node->data.range.end = t.current_tock;
          mv->state = MWState::Complete();
          mv->end_time = t.current_tock;
          mv->output = output;
        } else {
          break;
        }
      }
      node = node->parent;
    }
  return Zombie<cfg, Ret>(output);
  */
}

} // end of namespace ZombieInternal
