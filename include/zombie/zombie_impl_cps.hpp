#pragma once

#include <iostream>
#include <type_traits>
#include <set>

#include "zombie/zombie.hpp"
#include "zombie/common.hpp"
#include "zombie/trailokya.hpp"

namespace ZombieInternal {

namespace CPS {

template<const ZombieConfig& cfg>
MicroWave<cfg>::MicroWave(std::function<Trampoline::Output<Tock>(const std::vector<const void*>& in)>&& f,
                          const std::vector<Tock>& inputs,
                          const Tock& output,
                          const Tock& start_time,
                          const Tock& end_time,
                          const Space& space,
                          const Time& time_taken)
  : f(std::move(f)),
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
Trampoline::Output<Tock> MicroWave<cfg>::play(const std::function<Trampoline::Output<Tock>(const std::vector<const void*>& in)>& f,
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
MicroWavePtr<cfg>::MicroWavePtr(std::function<Trampoline::Output<Tock>(const std::vector<const void*>& in)>&& f,
                                const std::vector<Tock>& inputs,
                                const Tock& output,
                                const Tock& start_time,
                                const Tock& end_time,
                                const Space& space,
                                const Time& time_taken) :
  std::shared_ptr<MicroWave<cfg>>(std::make_shared<MicroWave<cfg>>(std::move(f), inputs, output, start_time, end_time, space, time_taken)) {
  auto& t = Trailokya<cfg>::get_trailokya();
  t.book.push(std::make_unique<RecomputeLater<cfg>>(start_time, *this), (*this)->cost());
}

template<const ZombieConfig& cfg>
void MicroWavePtr<cfg>::replay() {
  (*this)->replay();

  auto& t = Trailokya<cfg>::get_trailokya();
  t.book.push(std::make_unique<RecomputeLater<cfg>>((*this)->start_time, *this), (*this)->cost());
}

// todo: when in recompute mode, once the needed value is computed, we can skip the rest of the computation, jumping straight up.
template<const ZombieConfig& cfg>
Trampoline::Output<Tock> bindZombieRaw(std::function<Trampoline::Output<Tock>(const std::vector<const void*>&)>&& func, std::vector<Tock>&& in) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  auto default_path = [&](const Tock& min_end_time) {
    Tock start_time = t.current_tock++;
    std::tuple<Trampoline::Output<Tock>, ns, size_t> p = t.meter.measured([&](){ return MicroWave<cfg>::play(func, in); });
    Trampoline::Output<Tock> out = std::get<0>(p);
    ns time_taken = std::get<1>(p);
    size_t space_taken = std::get<2>(p);
    t.current_tock = std::max(min_end_time, t.current_tock);
    Tock end_time = t.current_tock;
    Tock out_tock = dynamic_cast<Trampoline::ReturnNode<Tock>*>(out.get())->t;
    t.akasha.put({start_time, end_time}, { MicroWavePtr<cfg>(std::move(func), in, out_tock, start_time, end_time, Space(space_taken), Time(time_taken)) });
    return out;
  };
  if (!t.akasha.has_precise(t.current_tock)) {
    return default_path(std::numeric_limits<Tock>::min());
  } else {
    const TockTreeData<typename Trailokya<cfg>::TockTreeElem>& n = t.akasha.get_precise_node(t.current_tock);
    std::shared_ptr<MicroWave<cfg>> mv = std::get<TockTreeElemKind::MicroWave>(n.value);
    Tock start_time = t.current_tock++;
    return MicroWave<cfg>::play(func, in);
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
    t.current_tock++;
    return ret_type(std::numeric_limits<Tock>::max());
  } else {
    std::function<Trampoline::Output<Tock>(const std::vector<const void*>&)> func =
      [f = std::forward<F>(f)](const std::vector<const void*> in) {
        auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
        std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
        return Result(std::apply([&](const Arg*... arg) { return f(*arg...); }, args)).o;
      };
    std::vector<Tock> in = {x.created_time...};
    Trampoline::Output<Tock> o = bindZombieRaw<cfg>(std::move(func), std::move(in));
    return ret_type(dynamic_cast<Trampoline::ReturnNode<Tock>*>(o.get())->t);
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
      std::vector<Tock> in = {x.created_time...};
      return bindZombieRaw<cfg>(std::move(func), std::move(in));
    };
    return ret_type { std::make_shared<Trampoline::TCNode<Tock>>(o) };
  }
}

template<const ZombieConfig& cfg, typename Ret, typename F, typename... Arg>
Zombie<cfg, Ret> bindZombieTC(F&& f, const Zombie<cfg, Arg>& ...x) {
  static_assert(std::is_same<decltype(f(std::declval<Arg>()...)), TCZombie<Ret>>::value, "result must be TCZombie");
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  Trampoline::Output<Tock> o = TailCall(std::forward<F>(f), x...).o;
  while(dynamic_cast<Trampoline::ReturnNode<Tock>*>(o.get()) == nullptr) {
    o = dynamic_cast<Trampoline::TCNode<Tock>*>(o.get())->func();
  }
  Tock output = dynamic_cast<Trampoline::ReturnNode<Tock>*>(o.get())->t;
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
    t.current_tock++;
    return ret_type(std::numeric_limits<Tock>::max());
  } else {
    std::vector<Tock> in;
    for (const EZombie<cfg>& ez : x) {
      in.push_back(ez.created_time);
    }
    std::function<Trampoline::Output<Tock>(const std::vector<const void*>&)> func =
      [f = std::forward<F>(f)](const std::vector<const void*> in) {
        return Result(f(in)).o;
      };
    Trampoline::Output<Tock> o = bindZombieRaw<cfg>(std::move(func), std::move(in));
    return ret_type(dynamic_cast<Trampoline::ReturnNode<Tock>*>(o.get())->t);
  }
}
} // end of namespace CPS

} // end of namespace ZombieInternal
