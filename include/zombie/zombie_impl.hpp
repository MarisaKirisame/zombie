#pragma once

#include <iostream>
#include <type_traits>
#include <set>

#include "zombie/zombie.hpp"
#include "zombie/common.hpp"
#include "zombie/trailokya.hpp"

namespace ZombieInternal {

template<const ZombieConfig& cfg>
MicroWave<cfg>::MicroWave(
  std::function<Tock(const std::vector<const void*>& in)>&& f,
  const std::vector<Tock>& inputs,
  const Tock& output,
  const Tock& start_time,
  const Tock& end_time,
  const Space& space,
  const Time& time_taken
) : f(std::move(f)),
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
    if (parent.index() == TockTreeElemKind::MicroWave)
        std::get<TockTreeElemKind::MicroWave>(parent)->used_by.push_back(start_time);
  }
}


template<const ZombieConfig& cfg>
Tock MicroWave<cfg>::play(const std::function<Tock(const std::vector<const void*>& in)>& f,
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
AffFunction MicroWave<cfg>::get_aff() const {
  return cfg.metric(last_accessed, time_taken, cost_of_set(), space_taken);
}



template<const ZombieConfig& cfg>
void MicroWave<cfg>::accessed() const {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  last_accessed = Time(t.meter.time());
  if (pool_index != -1) {
    assert(pool_index >= 0);
    t.book.set_aff(pool_index, get_aff());
  }
}


template<const ZombieConfig& cfg>
void MicroWave<cfg>::evict() {
  for (const Tock& t : inputs)
    merge_with(t);

  for (const Tock& t : used_by)
    merge_with(t);

  evicted = true;
}



template<const ZombieConfig& cfg>
Tock MicroWave<cfg>::root_of_set() const {
  return info_of_set().first;
}


template<const ZombieConfig& cfg>
Time MicroWave<cfg>::cost_of_set() const {
  // already evicted
  if (evicted)
    return info_of_set().second;

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
  for (const Tock& in : inputs)
    add_neighbor(in);
  for (const Tock& out : used_by)
    add_neighbor(out);

  return cost;
}


template<const ZombieConfig& cfg>
std::pair<Tock, Time> MicroWave<cfg>::info_of_set() const {
  if (_set_parent == start_time)
    return { start_time, _set_cost };

  auto& t = Trailokya<cfg>::get_trailokya();
  auto parent = std::get<TockTreeElemKind::MicroWave>(t.akasha.get_node(_set_parent).value);
  auto root_info = parent->info_of_set();
  _set_parent = root_info.first;

  return root_info;
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
MicroWavePtr<cfg>::MicroWavePtr(
  std::function<Tock(const std::vector<const void*>& in)>&& f,
  const std::vector<Tock>& inputs,
  const Tock& output,
  const Tock& start_time,
  const Tock& end_time,
  const Space& space,
  const Time& time_taken
) : std::shared_ptr<MicroWave<cfg>>(std::make_shared<MicroWave<cfg>>(std::move(f), inputs, output, start_time, end_time, space, time_taken)) {
  auto& t = Trailokya<cfg>::get_trailokya();
  t.book.push(std::make_unique<RecomputeLater<cfg>>(start_time, *this), (*this)->get_aff());
}

template<const ZombieConfig& cfg>
void MicroWavePtr<cfg>::replay() {
  (*this)->replay();

  auto& t = Trailokya<cfg>::get_trailokya();
  t.book.push(std::make_unique<RecomputeLater<cfg>>((*this)->start_time, *this), (*this)->get_aff());
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
  if (ret)
    return ret;

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
AffFunction RecomputeLater<cfg>::get_aff() const {
  return non_null(weak_ptr.lock())->get_aff();
}

template<const ZombieConfig& cfg>
Space RecomputeLater<cfg>::get_space() const {
  return non_null(weak_ptr.lock())->space_taken;
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

template<const ZombieConfig& cfg, typename ret_type>
ret_type bindZombieRaw(std::function<Tock(const std::vector<const void*>&)>&& func, std::vector<Tock>&& in) {
  static_assert(IsZombie<ret_type>::value, "should be zombie");
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  if (!t.akasha.has_precise(t.current_tock)) {
    Tock start_time = t.current_tock++;
    std::tuple<Tock, ns, size_t> p = t.meter.measured([&](){ return MicroWave<cfg>::play(func, in); });
    Tock out = std::get<0>(p);
    ns time_taken = std::get<1>(p);
    size_t space_taken = std::get<2>(p);
    Tock end_time = t.current_tock;
    ret_type ret(out);
    t.akasha.put({start_time, end_time}, { MicroWavePtr<cfg>(std::move(func), in, out, start_time, end_time, Space(space_taken), Time(time_taken)) });
    return ret;
  } else {
    const TockTreeData<typename Trailokya<cfg>::TockTreeElem>& n = t.akasha.get_precise_node(t.current_tock);
    t.current_tock = n.range.end;
    static_assert(IsZombie<ret_type>::value, "should be zombie");
    std::shared_ptr<MicroWave<cfg>> mv = std::get<TockTreeElemKind::MicroWave>(n.value);
    ret_type ret(mv->output);
    // we choose call-by-value because
    // 0: the original code evaluate in call by value, so there is likely no asymptotic speedup by calling call-by-need.
    // 1: calculating ret will force it's dependency, and call-by-value should provide better locality:
    //   to be precise, when this bindZombie is being replayed, it's dependency might be rematerialized for other use,
    //   thus it make sense to replay this bindZombie, to avoid the dependency being evicted later.
    // 2: rematerializing later require getting the GraveYard from the tock_tree,
    //   which require traversing a datastructure,
    //   while once context(zipper) is implemented, call-by-value doesnt need that.
    // of course, we could and should benchmark and see whether it should be call-by-value or call-by-need.
    constexpr bool call_by_value = true;
    if (call_by_value) {
      ret.shared_ptr();
    }
    return ret;
  }
}

template<const ZombieConfig& cfg, typename F, typename... Arg>
auto bindZombie(F&& f, const Zombie<cfg, Arg>& ...x) {
  using ret_type = decltype(f(std::declval<Arg>()...));
  std::function<Tock(const std::vector<const void*>&)> func =
    [f = std::forward<F>(f)](const std::vector<const void*> in) {
      auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
      std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
      auto z = std::apply([&](const Arg*... arg) { return f(*arg...); }, args);
      return z.created_time;
    };
  std::vector<Tock> in = {x.created_time...};
  return bindZombieRaw<cfg, ret_type>(std::move(func), std::move(in));
}

// While this function is not strictly necessary, as it could be replaced by binding and passing a Zombie<list<Any>>,
// doing so is clumsy to use and inefficient.
template<const ZombieConfig& cfg, typename F>
auto bindZombieUnTyped(F&& f, const std::vector<EZombie<cfg>>& x) {
  using ret_type = decltype(f(std::declval<std::vector<const void*>>()));
  std::vector<Tock> in;
  for (const EZombie<cfg>& ez : x) {
    in.push_back(ez.created_time);
  }
  std::function<Tock(const std::vector<const void*>&)> func =
    [f = std::forward<F>(f)](const std::vector<const void*> in) {
      auto z = f(in);
      return z.created_time;
    };
  return bindZombieRaw<cfg, ret_type>(std::move(func), std::move(in));
}

} // end of namespace ZombieInternal
