#pragma once

#include <memory>
#include <functional>
#include <vector>

#include "tock.hpp"
#include "trailokya.hpp"
#include "assert.hpp"

template<typename T>
T non_null(T&& x) {
  ASSERT(x);
  return std::forward<T>(x);
}

struct ScopeGuard {
  Trailokya& w;
  std::vector<Scope>& scopes;
  ScopeGuard(Trailokya& w) : w(w), scopes(w.scopes) {
    scopes.push_back(Scope());
  }
  ~ScopeGuard() {
    scopes.pop_back();
  }
};

// A MicroWave record a computation executed by bindZombie, to replay it.
// Note that a MicroWave may invoke more bindZombie, which may create MicroWave.
// When that happend, the outer MicroWave will not replay the inner one,
// And the metadata is measured accordingly.
// Note: MicroWave might be created or destroyed,
// Thus the work executed by the parent MicroWave will also change.
// The metadata must be updated accordingly.
struct MicroWave : Object {
  std::function<void(const std::vector<const void*>& in)> f;
  std::vector<tock> inputs;
  tock start_time;
  tock end_time;
  MicroWave(std::function<void(const std::vector<const void*>& in)>&& f,
	    const std::vector<tock>& inputs,
	    const tock& start_time,
	    const tock& end_time) :
    f(std::move(f)),
    inputs(inputs),
    start_time(start_time),
    end_time(end_time) { }
  void replay();
};

template <typename T>
bool weak_is_nullptr(std::weak_ptr<T> const& weak) {
  using wt = std::weak_ptr<T>;
  return !weak.owner_before(wt{}) && !wt{}.owner_before(weak);
}

// Manage a Zombie.
// At most one pointer is not a nullptr.
//
// When evictable is present,
// The managed is a normal Zombie, that might be evicted from memory.
// note that the weak_ptr will not be expired().
// this is because when Zombie die the accompanying GraveYard is removed.
//
// When holding is present,
// The managed is a Zombie which should not be evicted right now.
//
// When no pointer is present,
// This mean the Zombie is dead, but we would like to revive it.
// When the Zombie is eventually revived, holding should be filled,
// and the revive-requester should get the value and make it evictable.
//
// todo: it look like we can use only one pointer by unioning it.
// in such a case pointer tagging can be use to distinguish the three case. 
struct GraveYard : Object {
  std::weak_ptr<EZombieNode> evictable;
  std::shared_ptr<EZombieNode> holding;
  explicit GraveYard(const std::shared_ptr<EZombieNode>& ptr) : holding(ptr) { }
  explicit GraveYard() { }
  bool zombie_present() {
    return (!weak_is_nullptr(evictable)) || holding != nullptr;
  }
  // Does not revive if dead.
  std::shared_ptr<EZombieNode> summon() {
    if (!weak_is_nullptr(evictable)) {
      ASSERT(evictable.lock());
      return non_null(evictable.lock());
    } else if (holding != nullptr) {
      return non_null(holding);
    } else {
      return std::shared_ptr<EZombieNode>();
    }
  }
  // Arise my Zombie!
  std::shared_ptr<EZombieNode> arise(const tock_tree<std::unique_ptr<Object>>::Node& node) {
    auto ret = summon();
    if (ret) {
      return ret;
    } else {
      // At your side my GraveYard!
      dynamic_cast<MicroWave*>(non_null(node.parent)->value.get())->replay();
      ret = non_null(holding);
      make_evictable();
      return ret;
    }
  }
  void make_evictable() {
    ASSERT(holding);
    Trailokya::get_trailokya().book.insert(holding);
    evictable = holding;
    holding.reset();
  }
};


template<>
struct NotifyParentChanged<std::unique_ptr<Object>> {
  void operator()(std::unique_ptr<Object>& node, typename tock_tree<std::unique_ptr<Object>>::Node* parent) {
    Object* obj = node.get();
    if (GraveYard* gy = dynamic_cast<GraveYard*>(obj)) {
      if (gy->holding && parent != nullptr) {
	gy->make_evictable();
      }
    } else if (MicroWave* mw = dynamic_cast<MicroWave*>(obj)) { }
    else {
      // type not found
      ASSERT(false);
    }
  }
};

template<typename F, size_t... Is>
auto gen_tuple_impl(F func, std::index_sequence<Is...> ) {
  return std::make_tuple(func(Is)...);
}

template<size_t N, typename F>
auto gen_tuple(F func) {
  return gen_tuple_impl(func, std::make_index_sequence<N>{} );
}

struct EZombieNode : Object {
  tock created_time;
  EZombieNode(tock created_time) : created_time(created_time) { }
  ~EZombieNode() {
    auto& t = Trailokya::get_trailokya();
    if (!t.in_ragnarok) {
      t.akasha.get_precise_node(created_time).delete_node();
    }
  }
  ptrdiff_t pool_index = -1;
  virtual const void* get_ptr() const = 0;
};

template<>
struct BagObserver<std::shared_ptr<EZombieNode>> {
  void operator()(std::shared_ptr<EZombieNode>& t, size_t idx) {
    t->pool_index = idx;
  }
};

template<typename T>
struct ZombieNode : EZombieNode {
  T t;
  const void* get_ptr() const {
    return &t;
  }
  const T& get_ref() const {
    return t;
  }
  ZombieNode(ZombieNode<T>&& t) = delete;
  template<typename... Args>
  ZombieNode(tock created_time, Args&&... args) : EZombieNode(created_time), t(std::forward<Args>(args)...) { }
};

// todo: make tock a newtype
struct FromTock {
  tock created_time;
  FromTock(tock created_time) : created_time(created_time) { }
};

// todo: it could be a shared_ptr to skip registering in node.
// when that happend, we gain both space and time,
// at the lose of eviction granularity.

// the shared_ptr is stored in the evict list. when it evict something it simply drop the pointer.
// T should manage it's own memory:
// when T is construct, only then all memory is released.
// this mean T should not hold shared_ptr.
// T having Zombie zombie is allowed though.
template<typename T>
struct Zombie {
  static_assert(!std::is_reference_v<T>, "should not be a reference");
  tock created_time;
  mutable std::weak_ptr<ZombieNode<T>> ptr_cache;
  bool evictable() const {
    auto ptr = this->ptr().lock();
    if (ptr) {
      return ptr->pool_index != -1;
    } else {
      return false;
    }
  }
  bool unique() const {
    return ptr().use_count() ==  1;
  }
  void evict() {
    if (evictable()) {
      auto ptr = non_null(this->ptr().lock());
      Trailokya::get_trailokya().book.remove(ptr->pool_index);
    }
  }
  void force_evict() {
    ASSERT(evictable());
    evict();
  }
  void force_unique_evict() {
    ASSERT(evictable());
    ASSERT(unique());
    evict();
  }
  std::weak_ptr<ZombieNode<T>> ptr() const {
    if (ptr_cache.expired()) {
      Trailokya& t = Trailokya::get_trailokya();
      if (t.akasha.has_precise(created_time)) {
	GraveYard* gy = dynamic_cast<GraveYard*>(t.akasha.get_precise_node(created_time).value.get());
	ASSERT(gy != nullptr);
	ptr_cache = non_null(std::dynamic_pointer_cast<ZombieNode<T>>(gy->summon()));
      }
    }
    return ptr_cache.lock();
  }
  std::shared_ptr<ZombieNode<T>> shared_ptr() const {
    auto ret = ptr().lock();
    if (ret) {
      return ret;
    } else {
      auto& t = Trailokya::get_trailokya();
      if (!t.akasha.has_precise(created_time)) {
	t.akasha.put({created_time, created_time + 1}, std::make_unique<GraveYard>());
      }
      auto& n = t.akasha.get_precise_node(created_time);
      GraveYard* gy = non_null(dynamic_cast<GraveYard*>(n.value.get()));
      ret = non_null(std::dynamic_pointer_cast<ZombieNode<T>>(gy->arise(n)));
      ptr_cache = ret;
      return ret;
    }
  }
  template<typename... Args>
  void construct(Args&&... args) {
    Trailokya& t = Trailokya::get_trailokya();
    created_time = t.current_tock++;
    if (t.akasha.has_precise(created_time)) {
      auto& n = t.akasha.get_precise_node(created_time);
      GraveYard* gy = non_null(dynamic_cast<GraveYard*>(n.value.get()));
      if (!gy->zombie_present()) {
	gy->holding = std::make_shared<ZombieNode<T>>(created_time, std::forward<Args>(args)...);
      }
    } else {
      auto shared = std::make_shared<ZombieNode<T>>(created_time, std::forward<Args>(args)...);
      ptr_cache = shared;
      t.akasha.put({created_time, created_time + 1}, std::make_unique<GraveYard>(shared));
    }
  }
  template<typename... Args>
  Zombie(Args&&... args) {
    construct(std::forward<Args>(args)...);
  }
  Zombie(const T& t) {
    construct(t);
  }
  Zombie(T&& t) {
    construct(std::move(t));
  }
  Zombie(FromTock&& ft) : created_time(ft.created_time) { }
  Zombie(const FromTock& ft) : created_time(FromTock(ft)) { }
  T get_value() const {
    return shared_ptr()->t;
  }
};


template<typename T>
struct IsZombie : std::false_type { };

template<typename T>
struct IsZombie<Zombie<T>> : std::true_type { };

template<typename F, typename... Arg>
auto bindZombie(F&& f, const Zombie<Arg>& ...x) {
  Trailokya& t = Trailokya::get_trailokya();
  ScopeGuard sg(t);
  if (!t.akasha.has_precise(t.current_tock)) {
    std::tuple<std::shared_ptr<ZombieNode<Arg>>...> g(x.shared_ptr()...);
    auto y = std::apply([](const std::shared_ptr<ZombieNode<Arg>>&... g_){ return std::make_tuple<>(std::cref(g_->get_ref())...); }, g);
    tock start_time = t.current_tock++;
    auto ret = std::apply(f, y);
    static_assert(IsZombie<decltype(ret)>::value, "should be zombie");
    tock end_time = t.current_tock;
    ASSERT(end_time == ret.created_time + 1);
    std::vector<tock> in = {x.created_time...};
    std::function<void(const std::vector<const void*>&)> func =
      [f = std::forward<F>(f)](const std::vector<const void*> in) {
	auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
	std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
	std::apply([&](const Arg*... arg){ return f(*arg...); }, args);
      };
    t.akasha.put({start_time, end_time}, std::make_unique<MicroWave>(std::move(func), in, start_time, end_time));
    return std::move(ret);
  } else {
    auto& n = t.akasha.get_precise_node(t.current_tock);
    t.current_tock = n.range.second;
    using ret_type = decltype(f(std::declval<Arg>()...));
    static_assert(IsZombie<ret_type>::value, "should be zombie");
    ret_type ret(FromTock(t.current_tock - 1));
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
