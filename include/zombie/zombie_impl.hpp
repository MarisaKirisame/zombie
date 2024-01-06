#pragma once

#include <iostream>
#include <type_traits>
#include <set>

#include "zombie/zombie.hpp"
#include "zombie/common.hpp"
#include "zombie/trailokya.hpp"

namespace ZombieInternal {

template<const ZombieConfig& cfg>
MicroWave<cfg>::MicroWave(std::function<Output<Tock>(const std::vector<const void*>& in)>&& f,
                          const MWState& state,
                          const std::vector<Tock>& inputs,
                          const Tock& output,
                          const Tock& start_time,
                          const Tock& end_time,
                          const Space& space,
                          const Time& time_taken)
  : f(std::move(f)),
    state(state),
    inputs(inputs),
    output(output),
    start_time(start_time),
    end_time(end_time),
    space_taken(space),
    time_taken(time_taken),
    last_accessed(Trailokya<cfg>::get_trailokya().meter.time()),
    _set_parent(start_time),
    _set_cost(time_taken) {

  auto& t = Trailokya<cfg>::get_trailokya();
  for (const Tock& in : inputs) {
    auto parent = t.akasha.get_parent(in)->value;
    if (parent.index() == TockTreeElemKind::MicroWave) {
      std::get<TockTreeElemKind::MicroWave>(parent)->used_by.push_back(start_time);
    }
  }
}

template<const ZombieConfig& cfg>
Output<Tock> MicroWave<cfg>::play(const std::function<Output<Tock>(const std::vector<const void*>& in)>& f,
                                  const std::vector<Tock>& inputs) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  std::vector<std::shared_ptr<EZombieNode<cfg>>> input_zombie;
  std::vector<const void*> in;
  for (const Tock& input : inputs) {
    auto ezn = EZombie<cfg>(input).shared_ptr();
    in.push_back(ezn->get_ptr());
    input_zombie.push_back(ezn);
  }
  return f(in);
}

template<const ZombieConfig& cfg>
void MicroWave<cfg>::replay() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
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
  }
}

template<const ZombieConfig& cfg>
cost_t MicroWave<cfg>::cost() const {
  return cfg.metric(time_taken, cost_of_set(), space_taken);
}

template<const ZombieConfig& cfg>
void MicroWave<cfg>::accessed() const {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  last_accessed = Time(t.meter.time());
  if (pool_index != -1) {
    assert(pool_index >= 0);
    t.book.touch(pool_index);
  }
}

template<const ZombieConfig& cfg>
void MicroWave<cfg>::evict() {
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
  return info_of_set().first;
}

template<const ZombieConfig& cfg>
Time MicroWave<cfg>::cost_of_set() const {
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
  if (_set_parent == start_time) {
    return { start_time, _set_cost };
  } else {
    auto& t = Trailokya<cfg>::get_trailokya();
    auto parent = std::get<TockTreeElemKind::MicroWave>(t.akasha.get_node(_set_parent).value);
    auto root_info = parent->info_of_set();
    _set_parent = root_info.first;

    return root_info;
  }
}

template<const ZombieConfig& cfg>
void MicroWave<cfg>::merge_with(Tock other_tock) {
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
  root2->_set_cost = root1->_set_cost + root2->_set_cost;
}

template<const ZombieConfig& cfg>
MicroWavePtr<cfg>::MicroWavePtr(std::function<Output<Tock>(const std::vector<const void*>& in)>&& f,
                                const MWState& state,
                                const std::vector<Tock>& inputs,
                                const Tock& output,
                                const Tock& start_time,
                                const Tock& end_time,
                                const Space& space,
                                const Time& time_taken) :
  std::shared_ptr<MicroWave<cfg>>(std::make_shared<MicroWave<cfg>>(std::move(f), state, inputs, output, start_time, end_time, space, time_taken)) {
  auto& t = Trailokya<cfg>::get_trailokya();
  t.book.push(std::make_unique<RecomputeLater<cfg>>(start_time, *this), (*this)->cost());
}

template<const ZombieConfig& cfg>
void MicroWavePtr<cfg>::replay() {
  (*this)->replay();

  auto& t = Trailokya<cfg>::get_trailokya();
  t.book.push(std::make_unique<RecomputeLater<cfg>>((*this)->start_time, *this), (*this)->cost());
}

template<const ZombieConfig& cfg>
EZombieNode<cfg>::EZombieNode(Tock created_time)
  : created_time(created_time) { }

template<const ZombieConfig& cfg>
void EZombieNode<cfg>::accessed() const {
  auto parent = get_parent();
  if (parent)
    parent->accessed();
}

template<const ZombieConfig& cfg>
std::shared_ptr<MicroWave<cfg>> EZombieNode<cfg>::get_parent() const {
  auto ret = parent_cache.lock();
  if (ret) {
    return ret;
  }

  auto& t = Trailokya<cfg>::get_trailokya();
  auto parent = t.akasha.get_parent(created_time);
  if (!parent || parent->value.index() == TockTreeElemKind::Nothing) {
    parent_cache = std::weak_ptr<MicroWave<cfg>>();
    return nullptr;
  }

  ret = std::get<TockTreeElemKind::MicroWave>(parent->value);
  parent_cache = ret;
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

template<const ZombieConfig& cfg>
EZombie<cfg> EZombie<cfg>::Partial() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  t.current_tock++;
  return EZombie(std::numeric_limits<Tock>::max());
}

template<const ZombieConfig& cfg, typename T>
Zombie<cfg, T> Zombie<cfg, T>::Partial() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  t.current_tock++;
  return Zombie<cfg, T>(std::numeric_limits<Tock>::max());
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

template<typename F, size_t... Is>
auto gen_tuple_impl(F func, std::index_sequence<Is...>) {
  return std::make_tuple(func(Is)...);
}

template<size_t N, typename F>
auto gen_tuple(F func) {
  return gen_tuple_impl(func, std::make_index_sequence<N>{} );
}

template<typename T>
struct IsZombie : std::false_type { };

template<const ZombieConfig& cfg, typename T>
struct IsZombie<Zombie<cfg, T>> : std::true_type { };

template<typename T>
struct IsTCZombie : std::false_type { };

template<typename T>
struct IsTCZombie<TCZombie<T>> : std::true_type { };

// todo: when in recompute mode, once the needed value is computed, we can skip the rest of the computation, jumping straight up.
template<const ZombieConfig& cfg>
Output<Tock> bindZombieRaw(std::function<Output<Tock>(const std::vector<const void*>&)>&& func, std::vector<Tock>&& in) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  auto default_path = [&](const Tock& min_end_time) {
    Tock start_time = t.current_tock++;
    std::tuple<Output<Tock>, ns, size_t> p = t.meter.measured([&](){ return MicroWave<cfg>::play(func, in); });
    Output<Tock> out = std::get<0>(p);
    ns time_taken = std::get<1>(p);
    size_t space_taken = std::get<2>(p);
    t.current_tock = std::max(min_end_time, t.current_tock);
    bool is_tailcall = dynamic_cast<ReturnNode<Tock>*>(out.get()) == nullptr;
    if (is_tailcall) {
      // get the to-be-inserted MicroWave's parent, and set the end to that's end.
      Tock end_time = t.akasha.get_node(start_time).range.end;
      Tock out_tock = std::numeric_limits<Tock>::max();
      t.akasha.put({start_time, end_time}, { MicroWavePtr<cfg>(std::move(func), MWState::TailCall(), in, out_tock, start_time, end_time, Space(space_taken), Time(time_taken)) });
    } else if (t.tardis.is_partial) {
      Tock end_time = t.current_tock;
      Tock out_tock = std::numeric_limits<Tock>::max();
      t.akasha.put({start_time, end_time}, { MicroWavePtr<cfg>(std::move(func), MWState::Partial(), in, out_tock, start_time, end_time, Space(space_taken), Time(time_taken)) });
    } else {
      Tock end_time = t.current_tock;
      Tock out_tock = dynamic_cast<ReturnNode<Tock>*>(out.get())->t;
      MWState state = (!is_tailcall) ? MWState::Complete() : MWState::TailCall();
      t.akasha.put({start_time, end_time}, { MicroWavePtr<cfg>(std::move(func), MWState::Complete(), in, out_tock, start_time, end_time, Space(space_taken), Time(time_taken)) });
    }
    return out;
  };
  if (!t.akasha.has_precise(t.current_tock)) {
    return default_path(std::numeric_limits<Tock>::min());
  } else {
    const TockTreeData<typename Trailokya<cfg>::TockTreeElem>& n = t.akasha.get_precise_node(t.current_tock);
    std::shared_ptr<MicroWave<cfg>> mv = std::get<TockTreeElemKind::MicroWave>(n.value);
    if (mv->state == MWState::Complete()) {
      t.current_tock = n.range.end;
      auto ret(mv->output);
      // we choose call-by-need because it is more memory efficient.
      constexpr bool call_by_value = false;
      if (call_by_value) {
        EZombie<cfg>(ret).shared_ptr();
      }
      return std::make_shared<ReturnNode<Tock>>(ret);
    } else if (mv->state == MWState::TailCall()) {
      // replaying. we will not finish because our result is partial. no need to update the MicroWave in such case.
      Tock start_time = t.current_tock++;
      return MicroWave<cfg>::play(func, in);
    } else {
      assert(mv->state == MWState::Partial());
      // technically we dont need this, but I want to remove the shared_ptr. lets not setup trap for future us.
      Tock min_end_time = mv->end_time;
      t.akasha.remove_precise(t.current_tock);
      return default_path(min_end_time);
    }
  }
}

template<const ZombieConfig& cfg, typename F, typename... Arg>
auto bindZombie(F&& f, const Zombie<cfg, Arg>& ...x) {
  using ret_type = decltype(f(std::declval<Arg>()...));
  static_assert(IsZombie<ret_type>::value, "should be zombie");
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  // I hate we have the following partial-handling code at here, tc, and untyped.
  assert(t.current_tock != t.tardis.forward_at);
  if (t.current_tock > t.tardis.forward_at) {
    t.tardis.is_partial = true;
    // note how we cannot advance tock here: a bindZombie might only advance tock by 1, and Partial() already does such advancement.
    return ret_type::Partial();
  } else {
    std::function<Output<Tock>(const std::vector<const void*>&)> func =
      [f = std::forward<F>(f)](const std::vector<const void*> in) {
        auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
        std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
        return Result(std::apply([&](const Arg*... arg) { return f(*arg...); }, args)).o;
      };
    std::vector<Tock> in = {x.created_time...};
    Output<Tock> o = bindZombieRaw<cfg>(std::move(func), std::move(in));
    return ret_type(dynamic_cast<ReturnNode<Tock>*>(o.get())->t);
  }
}

template<const ZombieConfig& cfg, typename F, typename... Arg>
auto TailCall(F&& f, const Zombie<cfg, Arg>& ...x) {
  using ret_type = decltype(f(std::declval<Arg>()...));
  static_assert(IsTCZombie<ret_type>::value, "should be TCZombie");
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  assert(t.current_tock != t.tardis.forward_at);
  if (t.current_tock > t.tardis.forward_at) {
    t.tardis.is_partial = true;
    // note how we cannot advance tock here: a bindZombie might only advance tock by 1, and Partial() already does such advancement.
    return ret_type { std::make_shared<ReturnNode<Tock>>(EZombie<cfg>::Partial().created_time) };
  } else {
    std::function<Output<Tock>()> o = [f = std::forward<F>(f), x...]() {
      std::function<Output<Tock>(const std::vector<const void*>&)> func =
        [f = std::move(f)](const std::vector<const void*> in) {
          auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
          std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
          return std::apply([&](const Arg*... arg) { return f(*arg...); }, args).o;
        };
      std::vector<Tock> in = {x.created_time...};
      return bindZombieRaw<cfg>(std::move(func), std::move(in));
    };
    return ret_type { std::make_shared<TCNode<Tock>>(o) };
  }
}

template<const ZombieConfig& cfg, typename Ret, typename F, typename... Arg>
Zombie<cfg, Ret> bindZombieTC(F&& f, const Zombie<cfg, Arg>& ...x) {
  static_assert(std::is_same<decltype(f(std::declval<Arg>()...)), TCZombie<Ret>>::value, "result must be TCZombie");
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  Output<Tock> o = TailCall(std::forward<F>(f), x...).o;
  while(dynamic_cast<ReturnNode<Tock>*>(o.get()) == nullptr) {
    o = dynamic_cast<TCNode<Tock>*>(o.get())->func();
  }
  Tock output = dynamic_cast<ReturnNode<Tock>*>(o.get())->t;
  // Why do we only fix to complete and not to partial?
  // TailCall mean "we are still actively working on it."
  // It will eventually get fixed once that return, so there is no need to touch it.
  // Another way of thinking about it is, changing it to Partial does not have any benefit.
  if (!t.tardis.is_partial) {
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
  }
  return Zombie<cfg, Ret>(output);
}

// While this function is not strictly necessary, as it could be replaced by binding and passing a Zombie<list<Any>>,
// doing so is clumsy to use and inefficient.
template<const ZombieConfig& cfg, typename F>
auto bindZombieUnTyped(F&& f, const std::vector<EZombie<cfg>>& x) {
  using ret_type = decltype(f(std::declval<std::vector<const void*>>()));
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  assert(t.current_tock != t.tardis.forward_at);
  if (t.current_tock > t.tardis.forward_at) {
    t.tardis.is_partial = true;
    // note how we cannot advance tock here: a bindZombie might only advance tock by 1, and Partial() already does such advancement.
    return ret_type::Partial();
  } else {
    std::vector<Tock> in;
    for (const EZombie<cfg>& ez : x) {
      in.push_back(ez.created_time);
    }
    std::function<Output<Tock>(const std::vector<const void*>&)> func =
      [f = std::forward<F>(f)](const std::vector<const void*> in) {
        return Result(f(in)).o;
      };
    Output<Tock> o = bindZombieRaw<cfg>(std::move(func), std::move(in));
    return ret_type(dynamic_cast<ReturnNode<Tock>*>(o.get())->t);
  }
}
} // end of namespace ZombieInternal
