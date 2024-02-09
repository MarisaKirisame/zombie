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
  size_t s = this->ez.size();
  this->ez.clear();
  UF<Time> cost(time_taken);
  cost.merge(this->evicted_compute_dependents);
  evicted_data_dependents.merge(cost);

  for (const Tock& input: dependencies) {
    auto* n = t.akasha.find_le_node(input);
    if (n->v->end_t > input) {
      // we found the node. it is a data dependency.
      if (auto* ptr = dynamic_cast<FullContextNode<cfg>*>(n->v.get())) {
        ptr->evicted_data_dependents.insert(cost);
      }
    } else {
      // the node is evicted.
      // so there is a compute dependency between the node and the node that hold this value.
      n->v->evicted_compute_dependents.merge(cost);
    }
  }

  auto* parent_node = t.akasha.find_precise_node(this->start_t)->parent;
  auto parent_context = parent_node->v;
  parent_context->evicted_compute_dependents.merge(cost);

  //std::cout << "evicting " << this->start_t << ", cost: " << cost.value().count() << " gd heap size: " << t.book.size() << std::endl;
  // this line delete this;
  t.akasha.remove_precise(this->start_t);
}

template<const ZombieConfig& cfg>
cost_t FullContextNode<cfg>::cost() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  std::unordered_set<UF<Time>> counted = {this->evicted_compute_dependents};
  Time cost = time_taken + this->evicted_compute_dependents.value() + evicted_data_dependents.sum(counted);
  for (const Tock& input: dependencies) {
    auto uf = (*(t.akasha.find_le(input)))->evicted_compute_dependents;
    if (counted.insert(uf).second) {
      cost += uf.value();
    }
  }
  return cfg.metric(time_taken, cost, Space(this->space_taken));
}

template<const ZombieConfig& cfg>
void FullContextNode<cfg>::evict_individual(const Tock& t) {
  this->ez[tock_to_index(t, this->start_t)].reset();
}

template<const ZombieConfig& cfg>
ContextNode<cfg>::ContextNode(const Tock& start_t, const Tock& end_t,
                              std::vector<std::shared_ptr<EZombieNode<cfg>>>&& ez,
                              const size_t& sp,
                              const Replayer<cfg>& rep) :
  start_t(start_t),
  end_t(end_t),
  ez(std::move(ez)),
  space_taken(sp),
  end_rep(rep) {
  if (start_t + this->ez.size() + 1 != end_t) {
    std::cout << start_t << " " << this->ez.size() << " " << end_t << std::endl;
  }
  assert(start_t + this->ez.size() + 1 == end_t);

  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  auto* n = t.akasha.find_le(start_t);
  if (n != nullptr && (*n)->end_t == start_t) {
    (*n)->evicted_compute_dependents = UF<Time>(0);
  } else {
    // this happens when n is a nullptr, it is a rootrecordnode, or when it's parent was freshly-evicted.
    // std::cout << "weird but possible" << std::endl;
  }
}

template<const ZombieConfig& cfg>
void ContextNode<cfg>::replay() {
  Time cost = evicted_compute_dependents.value();
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  Tock tock = t.current_tock;
  assert(this->end_t < t.replay.forward_at);
  Tock from = this->end_t;
  /*std::cout << "replaying from [" << this->start_t << ", " << this->end_t << ")"
            << " to " << t.replay.forward_at
            << " diff(/32) " << (t.replay.forward_at - from).tock / 32 + 1
            << " requested at " << t.current_tock
            << ", cost " << cost.count() << std::endl;*/
  // this does not look absolutely right, but the problem seems super complex.
  // lets come back later.
  // also have to goes before actual playing as that steal.
  bracket(
    [&]() {
      t.current_tock = from;
      t.records.push_back(std::make_shared<HeadRecordNode<cfg>>(this->end_rep));
    },
    [&]() {
      while(*(t.replay.forward_to) == nullptr) {
        assert(t.current_tock <= t.replay.forward_at);
        Tock old_tock = t.current_tock;
        t.records.back()->play();
        assert(old_tock < t.current_tock);
        t.each_step();
      }
    },
    [&]() {
      // assert we had not done any extra work
      assert(t.records.back()->ez.empty());
      t.records.pop_back();
      t.current_tock = tock;
    });
  //std::cout << "replaying done!" << std::endl;
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
            [&]() { t.meter.block([&](){
              auto* n = t.akasha.find_le_node(created_time);
              if (!(n->v->end_t < created_time)) {
                n = n->parent;
              }
              n->v->replay();
            }); },
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
  if (this->created_time != std::numeric_limits<Tock>::max()) {
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
void RecordNode<cfg>::suspend(const Replayer<cfg>& rep) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  this->suspended(rep);
  t.records.push_back(std::make_shared<HeadRecordNode<cfg>>(rep));
}

template<const ZombieConfig& cfg>
void RecordNode<cfg>::finish(const ExternalEZombie<cfg>& z) {
  this->completed(nullptr);
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  t.records.back() = std::make_shared<ValueRecordNode<cfg>>(ExternalEZombie<cfg>(z));
}

template<const ZombieConfig& cfg>
void RecordNode<cfg>::complete(const Replayer<cfg>& rep) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  this->completed(rep);
  // this line delete this;
  t.records.pop_back();
  t.records.back()->resumed();
}

template<const ZombieConfig& cfg>
ExternalEZombie<cfg> RecordNode<cfg>::pop_value() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  auto ret = this->get_value();
  t.records.pop_back();
  t.records.back()->resumed();
  return ret;
}

template<const ZombieConfig& cfg>
void RootRecordNode<cfg>::resumed() {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  assert(this->ez.empty());
  this->t = tick<cfg>();
}

template<const ZombieConfig& cfg>
HeadRecordNode<cfg>::HeadRecordNode(const Replayer<cfg>& rep) :
  rep(rep),
  start_time(Trailokya<cfg>::get_trailokya().meter.time()) { }

template<const ZombieConfig& cfg>
FullContextNode<cfg>::FullContextNode(const Tock& start_t,
                                      const Tock& end_t,
                                      std::vector<std::shared_ptr<EZombieNode<cfg>>>&& ez,
                                      const size_t& sp,
                                      const Time& time_taken,
                                      const Replayer<cfg>& rep,
                                      std::vector<Tock>&& deps) :
  ContextNode<cfg>(start_t, end_t, std::move(ez), sp, rep),
  time_taken(time_taken),
  last_accessed(Trailokya<cfg>::get_trailokya().meter.raw_time()),
  dependencies(std::move(deps)) { }

template<const ZombieConfig& cfg>
void RootRecordNode<cfg>::suspended(const Replayer<cfg>& rep) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  t.akasha.insert(this->t, std::make_shared<RootContextNode<cfg>>(this->t, t.current_tock,
                                                                  std::move(this->ez), this->space_taken, rep));
}

template<const ZombieConfig& cfg>
void HeadRecordNode<cfg>::completed(const Replayer<cfg>& rep) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  assert(this->t < t.replay.forward_at);
  std::vector<Tock> deps;
  for (const auto& i: this->rep->in) {
    this->dependencies.insert(i.created_time);
  }
  for (const auto& i: this->dependencies) {
    deps.push_back(i);
  }
  auto time_taken = Time(Trailokya<cfg>::get_trailokya().meter.time()) - start_time;
  // std::cout << time_taken.count() << std::endl;
  auto fc = std::make_shared<FullContextNode<cfg>>(this->t,
                                                   t.current_tock,
                                                   std::move(this->ez),
                                                   this->space_taken,
                                                   time_taken,
                                                   rep,
                                                   std::move(deps));
  t.book.push(std::make_unique<RecomputeLater<cfg>>(fc), fc->cost());
  //std::cout << "inserting: " << this->t << ", cost: " << fc->cost() << std::endl;
  t.akasha.insert(this->t, fc);
}

template<const ZombieConfig& cfg>
Tock tick() {
  return Trailokya<cfg>::get_trailokya().current_tock++;
}

template<const ZombieConfig& cfg>
void HeadRecordNode<cfg>::play() {
  assert(!played);
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  std::vector<std::shared_ptr<EZombieNode<cfg>>> storage;
  std::vector<const void*> in;
  for (const EZombie<cfg>& input : rep->in) {
    storage.push_back(input.shared_ptr());
    in.push_back(storage.back()->get_ptr());
  }
  (rep->f)(in);
  played = true;
}

template<const ZombieConfig& cfg, typename F, typename... Arg>
auto bindZombie(F&& f, const Zombie<cfg, Arg>& ...x) {
  using ret_type = decltype(f(std::declval<Arg>()...));
  static_assert(IsExternalZombie<ret_type>::value, "should be zombie");
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  assert(t.current_tock != t.replay.forward_at);
  if (t.current_tock < t.replay.forward_at) {
    replay_func<cfg> func =
      [f = std::forward<F>(f)](const std::vector<const void*> in) {
        Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
        auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
        std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
        ExternalEZombie<cfg> eez(std::apply([&](const Arg*... arg) { return f(*arg...); }, args));
        t.records.back()->finish(std::move(eez));
      };
    std::vector<EZombie<cfg>> in = {x...};
    t.records.back()->suspend(std::make_shared<ReplayerNode<cfg>>(std::move(func), std::move(in)));
    t.records.back()->play();
    ExternalEZombie<cfg> ez = t.records.back()->pop_value();
    return ret_type(std::move(ez));
  } else {
    return ret_type(std::numeric_limits<Tock>::max());
  }
}

template<const ZombieConfig& cfg, typename T>
TCZombie<cfg, T>::TCZombie(const ExternalZombie<cfg, T>& z) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  t.records.back()->finish(z);
}

template<const ZombieConfig& cfg, typename T>
TCZombie<cfg, T>::TCZombie(ExternalZombie<cfg, T>&& z) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  t.records.back()->finish(std::move(z));
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
TCZombie<cfg, T> ToTC(ExternalZombie<cfg, T>&& z) {
  return TCZombie<cfg, T>(std::move(z));
}

template<const ZombieConfig& cfg, typename T>
TCZombie<cfg, T> ToTC(const ExternalZombie<cfg, T>& z) {
  return TCZombie<cfg, T>(z);
}

template<const ZombieConfig& cfg, typename F, typename... Arg>
Replayer<cfg> InitTailCall(F&& f, const Zombie<cfg, Arg>& ...x) {
  using result_type = decltype(ToTC(f(std::declval<Arg>()...)));
  static_assert(IsTCZombie<result_type>::value, "should be TCZombie");
  replay_func<cfg> func =
    [f = std::forward<F>(f)](const std::vector<const void*> in) {
      auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
      std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
      std::apply([&](const Arg*... arg) { return ToTC(f(*arg...)); }, args);
    };
  return std::make_shared<ReplayerNode<cfg>>(std::move(func), std::vector<EZombie<cfg>>{x...});
}

template<const ZombieConfig& cfg>
void HeadRecordNode<cfg>::tailcall(const Replayer<cfg>& rep) {
  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  t.records.back()->completed(rep);
  // this line delete this;
  t.records.back() = std::make_shared<HeadRecordNode<cfg>>(rep);
}

template<typename T>
struct CaptureGuard {
  // technically one can still capture a boolean, but no big deal!
  // we only dont want (transitive) capturing of zombie, and possibly other big objects.
  static_assert(sizeof(std::remove_reference_t<T>) == 1);
};

template<const ZombieConfig& cfg, typename F, typename... Arg>
auto TailCall(F&& f, const Zombie<cfg, Arg>& ...x) {
  //CaptureGuard<F> no_capture;
  using result_type = decltype(ToTC(f(std::declval<Arg>()...)));
  static_assert(IsTCZombie<result_type>::value, "should be TCZombie");

  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  if (t.records.back()->is_tailcall() && t.current_tock - t.records.back()->t < 32) {
    for (const Tock& tock: std::vector<Tock>{x.created_time...}) {
      t.records.back()->register_unrolled(tock);
    }
    // this code does not live in tailcall() member function because we have to do multiple dispatch to do so.
    // note we cannot trampoline this code - doing so make complete() excute when it cannot.
    // std::vector<std::shared_ptr<EZombieNode<cfg>>> storage = {x.shared_ptr()...};
    // note that we do not need the above as it's lifetime is extended until execution finished.
    // this is very tricky, so I had decided to keep the commented code and talk about it -
    // basically we are doing a very subtle optimization.
    return ToTC(f(x.shared_ptr()->get_ref()...));
  } else {
    replay_func<cfg> func =
      [f = std::forward<F>(f)](const std::vector<const void*> in) {
        auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
        std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
        std::apply([&](const Arg*... arg) { return ToTC(f(*arg...)); }, args);
      };
    Replayer<cfg> replayer = std::make_shared<ReplayerNode<cfg>>(std::move(func), std::vector<EZombie<cfg>>{x...});
    t.records.back()->tailcall(replayer);
    return result_type();
  }
}

template<typename T>
struct TCZombieToExternalZombie { };

template<const ZombieConfig& cfg, typename T>
struct TCZombieToExternalZombie<TCZombie<cfg, T>> {
  using type = ExternalZombie<cfg, T>;
};

template<const ZombieConfig& cfg, typename F, typename... Arg>
auto bindZombieTC(F&& f, const Zombie<cfg, Arg>& ...x) {
  using tc_result_type = decltype(f(std::declval<Arg>()...));
  // static_assert(IsZombie<tc_result_type>::value, "result must be be TCZombie");
  using result_type = TCZombieToExternalZombie<tc_result_type>::type;

  Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
  Replayer<cfg> rep = InitTailCall<cfg>(std::forward<F>(f), x...);
  t.records.back()->suspend(rep);

  while(!t.records.back()->is_value()) {
    Tock old_tock = t.current_tock;
    t.records.back()->play();
    assert(old_tock < t.current_tock);
    t.each_step();
  }

  ExternalEZombie<cfg> ret = t.records.back()->pop_value();
  return result_type(std::move(ret));
}

} // end of namespace ZombieInternal
