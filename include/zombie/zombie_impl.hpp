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
TCZombie<cfg, T>::TCZombie(Zombie<cfg, T>&& z) : o(std::make_shared<Trampoline::ReturnNode<EZombie<cfg>>>(EZombie<cfg>(std::move(z)))) { }

template<const ZombieConfig &cfg, typename T>
TCZombie<cfg, T>::TCZombie(Tock&& t) : o(std::make_shared<Trampoline::ReturnNode<EZombie<cfg>>>(EZombie<cfg>(std::move(t)))) { }

template<const ZombieConfig &cfg, typename T>
TCZombie<cfg, T>::TCZombie(const Tock& t) : o(std::make_shared<Trampoline::ReturnNode<EZombie<cfg>>>(EZombie<cfg>(t))) { }

template<const ZombieConfig &cfg, typename T>
TCZombie<cfg, T>::TCZombie(const Trampoline::Output<EZombie<cfg>>& o) : o(o) { }

template<const ZombieConfig &cfg, typename T>
TCZombie<cfg, T>::TCZombie(Trampoline::Output<EZombie<cfg>>&& o) : o(std::move(o)) { }

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
                                      const Tock& t) :
  ContextNode<cfg>(std::move(ez), sp),
  time_taken(time_taken),
  f(std::move(f)),
  inputs(std::move(inputs)),
  t(t),
  last_accessed(Trailokya<cfg>::get_trailokya().meter.raw_time()) { }

template<const ZombieConfig& cfg>
FullContextNode<cfg>::~FullContextNode() {
  // if (pool_index != -1) {
  //   assert(pool_index >= 0);
  //   Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  //   t.book.heap.remove(pool_index);
  // }
  // we decided to not do this. instead pool_index might hold stale pointers.
}

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
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  this->ez.clear();
  auto* self = t.akasha.find_precise_node(this->t);
  if (self->parent->v->is_tailcall()) {
    t.akasha.remove_precise(this->t);
  }
}

template<const ZombieConfig& cfg>
void FullContextNode<cfg>::evict_individual(const Tock& t) {
  this->ez[tock_to_index(t, this->t)].reset();
}

template<const ZombieConfig& cfg>
void FullContextNode<cfg>::replay() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  Tock tock = t.current_tock;
  assert(this->t < t.replay.forward_at);
  bracket(
    [&]() {
      t.current_tock = this->t;
      t.records.push_back(std::make_shared<HeadRecordNode<cfg>>(std::move(f), std::move(inputs)));
      // be careful! this code will delete this.
      t.akasha.remove_precise(this->t);
    },
    [&]() {
      auto trampoline = t.records.back()->play();
      while (t.current_tock <= t.replay.forward_at) {
        assert(!trampoline->is_return());
        trampoline = trampoline->from_tailcall()();
      }
      assert(*(t.replay.forward_to) != nullptr);
    },
    [&]() {
      t.records.back()->completed();
      t.records.pop_back();
      // must go after completed(), as it do stuff to current_tock
      t.current_tock = tock;
    });
}

template<const ZombieConfig& cfg>
cost_t FullContextNode<cfg>::cost() {
  return cfg.metric(time_taken, time_taken, Space(this->space_taken));
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
  if (auto ptr = weak_ptr.lock()) {
    return ptr->cost();
  } else {
    return 0;
  }
}

template<const ZombieConfig& cfg>
void RecomputeLater<cfg>::evict() {
  if (auto ptr = weak_ptr.lock()) {
    ptr->evict();
  }
}

template<const ZombieConfig& cfg>
void EZombie<cfg>::evict() {
  if (evictable()) {
    this->ptr().lock()->get_context()->evict_individual(this->created_time);
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
    bracket([&]() { t.replay = Replay<cfg> { created_time, &strong }; },
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
    auto& record = t.records.back();
    assert(tock_to_index(this->created_time, record->t) == record->ez.size());
    record->ez.push_back(shared);
    record->space_taken += GetSize<T>()(shared->t);
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
void RecordNode<cfg>::suspend(const std::shared_ptr<RecordNode<cfg>>& rec) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  this->suspended();
  t.records.push_back(rec);
}

template<const ZombieConfig& cfg>
void RecordNode<cfg>::complete() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  this->completed();
  t.records.pop_back();
  t.records.back()->resumed();
}

template<const ZombieConfig& cfg>
void RecordNode<cfg>::replay_finished() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  this->completed();
  t.records.pop_back();
}

template<const ZombieConfig& cfg>
void RootRecordNode<cfg>::suspended() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  t.akasha.insert(this->t, std::make_shared<RootContextNode<cfg>>(std::move(this->ez), this->space_taken));
}

template<const ZombieConfig& cfg>
void RootRecordNode<cfg>::resumed() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  assert(this->ez.empty());
  this->t = tick<cfg>();
}

template<const ZombieConfig& cfg>
HeadRecordNode<cfg>::HeadRecordNode(std::function<Trampoline::Output<EZombie<cfg>>(const std::vector<const void*>& in)>&& f, std::vector<EZombie<cfg>>&& inputs) :
  f(std::move(f)),
  inputs(std::move(inputs)),
  start_time(Trailokya<cfg>::get_trailokya().meter.time()) { }

template<const ZombieConfig& cfg>
void HeadRecordNode<cfg>::completed() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  assert(this->t != t.replay.forward_at);
  if (this->t < t.replay.forward_at) { // otherwise it might be partial.
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
Tock tick() {
  return Trailokya<cfg>::get_trailokya().current_tock++;
}

template<const ZombieConfig& cfg>
Trampoline::Output<EZombie<cfg>> HeadRecordNode<cfg>::play() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  std::vector<std::shared_ptr<EZombieNode<cfg>>> storage;
  std::vector<const void*> in;
  for (const EZombie<cfg>& input : inputs) {
    storage.push_back(input.shared_ptr());
    in.push_back(storage.back()->get_ptr());
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

    t.records.back()->suspend(std::make_shared<HeadRecordNode<cfg>>(std::move(func), std::move(in)));
    Trampoline::Output<EZombie<cfg>> o = t.records.back()->play();
    t.records.back()->complete();
    return ret_type(dynamic_cast<Trampoline::ReturnNode<EZombie<cfg>>*>(o.get())->t);
  } else {
    return ret_type(std::numeric_limits<Tock>::max());
  }
}

template<const ZombieConfig& cfg, typename T>
TCZombie<cfg, T> ToTC(TCZombie<cfg, T>&& t) {
  return t;
}

template<const ZombieConfig& cfg, typename T>
TCZombie<cfg, T> ToTC(const TCZombie<cfg, T>& t) {
  return t;
}

template<const ZombieConfig& cfg, typename T>
TCZombie<cfg, T> ToTC(Zombie<cfg, T>&& t) {
  return Result(t);
}

template<const ZombieConfig& cfg, typename T>
TCZombie<cfg, T> ToTC(const Zombie<cfg, T>& t) {
  return Result(t);
}

template<const ZombieConfig& cfg, typename F, typename... Arg>
Record<cfg> InitTailCall(F&& f, const Zombie<cfg, Arg>& ...x) {
  using result_type = decltype(ToTC(f(std::declval<Arg>()...)));
  static_assert(IsTCZombie<result_type>::value, "should be TCZombie");
  std::function<Trampoline::Output<EZombie<cfg>>(const std::vector<const void*>&)> func =
    [f = std::forward<F>(f)](const std::vector<const void*> in) {
      auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
      std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
      return std::apply([&](const Arg*... arg) { return ToTC(f(*arg...)); }, args).o;
    };
  return std::make_shared<HeadRecordNode<cfg>>(std::move(func), std::vector<EZombie<cfg>>{x...});
}

template<const ZombieConfig& cfg>
Trampoline::Output<EZombie<cfg>> HeadRecordNode<cfg>::tailcall(std::function<Trampoline::Output<EZombie<cfg>>(const std::vector<const void*>& in)>&& f,
                                                               std::vector<EZombie<cfg>>&& in) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  this->completed();
  t.records.back() = std::make_shared<HeadRecordNode<cfg>>(std::move(f), std::move(in));
  return std::make_shared<Trampoline::TCNode<EZombie<cfg>>>([](){
    Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
    return t.records.back()->play();
  });
}

template<const ZombieConfig& cfg, typename F, typename... Arg>
auto TailCall(F&& f, const Zombie<cfg, Arg>& ...x) {
  using result_type = decltype(ToTC(f(std::declval<Arg>()...)));
  static_assert(IsTCZombie<result_type>::value, "should be TCZombie");

  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  if (t.current_tock <= t.replay.forward_at) {
    assert(t.records.back()->t < t.current_tock);
    if (t.records.back()->is_tailcall() && t.current_tock - t.records.back()->t < 32) {
      // this code does not live in tailcall() member function because we have to do multiple dispatch to do so.
      // note we cannot trampoline this code - doing so make complete() excute when it cannot.
      // std::vector<std::shared_ptr<EZombieNode<cfg>>> storage = {x.shared_ptr()...};
      // note that we do not need the above as it's lifetime is extended until execution finished.
      // this is very tricky, so I had decided to keep the commented code and talk about it -
      // basically we are doing a very subtle optimization.
      return ToTC(f(x.shared_ptr()->get_ref()...));
    } else {
      assert(t.current_tock != t.replay.forward_at);
      std::function<Trampoline::Output<EZombie<cfg>>(const std::vector<const void*>&)> func =
        [f = std::forward<F>(f)](const std::vector<const void*> in) {
          auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
          std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
          return std::apply([&](const Arg*... arg) { return ToTC(f(*arg...)); }, args).o;
        };
      return result_type(t.records.back()->tailcall(std::move(func), std::vector<EZombie<cfg>>{x...}));
    }
  } else {
    return result_type(std::numeric_limits<Tock>::max());
  }
}

template<const ZombieConfig& cfg, typename F, typename... Arg>
auto bindZombieTC(F&& f, const Zombie<cfg, Arg>& ...x) {
  using result_type = decltype(ToTC(f(std::declval<Arg>()...)));
  static_assert(IsTCZombie<result_type>::value, "result must be be TCZombie");

  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  Record<cfg> record = InitTailCall<cfg>(std::forward<F>(f), x...);
  t.records.back()->suspend(record);

  auto trampoline = t.records.back()->play();

  while(!trampoline->is_return()) {
    trampoline = trampoline->from_tailcall()();
    t.each_tc();
  }

  Trampoline::Output<EZombie<cfg>> o = t.records.back()->play();
  t.records.back()->complete();
  return Zombie<cfg, typename TCZombieInner<result_type>::type_t>(trampoline->from_return());
}

} // end of namespace ZombieInternal
