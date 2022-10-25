#pragma once

#include <memory>
#include <functional>

#include "tock.hpp"
#include "world.hpp"

struct ScopeGuard {
  World& w;
  std::vector<Scope>& scopes;
  ScopeGuard(World& w) : w(w), scopes(w.scopes) {
    scopes.push_back(Scope());
  }
  ~ScopeGuard() {
    scopes.pop_back();
  }
};

/*
#include <functional>
#include <unordered_map>
#include <vector>
#include <optional>
#include <iostream>
#include <map>
#include <cassert>
#include <type_traits>
#include <any>

#include "assert.hpp"
#include "phantom.hpp"

struct Computer : Object {
  std::function<void(const std::vector<const void*>& in)> f;
  std::vector<tock> input;
  tock output;
  size_t memory;
  int64_t compute_cost;
  Computer(std::function<void(const std::vector<const void*>& in)>&& f,
           const std::vector<tock>& input,
           const tock& output) :
    f(std::move(f)),
    input(input),
    output(output) { }
  void replay(const tock& start_at) {
    World& w = World::get_world();
    struct Tardis {
      World& w;
      tock old_tock;
      Tardis(World& w, const tock& new_tock) : w(w), old_tock(w.current_tock) {
        w.current_tock = new_tock;
      }
      ~Tardis() {
        w.current_tock = old_tock;
      }
    } t(w, start_at);
    ScopeGuard sg(World::get_world());
    w.current_tock++;
    // note that the pointer will be stale if the zombie are destructed, or moved, inside bindZombie.
    // so - dont do that!
    // only move zombie that you freshly created,
    // and only destruct zombie at destructor.
    std::vector<EMonster*> monster;
    std::vector<std::unique_ptr<Phantom>> phantom;
    for (const tock& t : input) {
      if (w.record.has_precise(t)) {
        monster.push_back(dynamic_cast<EMonster*>(w.record.get_precise_node(t).value));
      } else {
        auto p = std::make_unique<Phantom>(t);
        monster.push_back(p.get());
        phantom.push_back(std::move(p));
      }
    }
    std::vector<std::unique_ptr<EGuard>> guards;
    for (EMonster* em : monster) {
      guards.push_back(std::make_unique<EGuard>(em));
    }
    std::vector<const void*> in;
    for (const auto& z : guards) {
      in.push_back(z->get_ptr());
    }
    f(in);
  }
};

// T should manage it's own memory:
// when T is construct, only then all memory is released.
// this mean T should no contain Zombie or shared_ptr.
// however, such cases is not incorrect, it merely mess with uncompute/recompute profitability a bit.
template<typename T>
struct Zombie : EMonster {
private:
  //Zombie() : created_time(-1) { }
  Zombie<T>& operator=(Zombie<T>&& z) {
    t = std::move(z.t);
    z.t.reset();
    created_time = std::move(z.created_time);
    z.created_time = 0;
    World::get_world().record.get_precise_node(created_time).value = this;
    return *this;
  }
  Zombie<T>& operator=(const Zombie<T>&) = delete;
  template<typename F, typename... Arg>
  friend auto bindZombie(F&& f, const Zombie<Arg>&... x);
public:
  // 0 for moved Zombie, positive for normal, unique zombie, negative forward to corresponding positive.
  tock created_time;

  Zombie(Zombie<T>&& z) {
    (*this) = std::move(z);
  }

  Zombie(const Zombie<T>& z) = delete;

  ~Zombie() {
    if (created_time > 0) {
      World::get_world().record.get_precise_node(created_time).delete_node();
    }
  }

  const T& unsafe_get() const {
    return  t.value();
  }

  mutable int64_t lock_cnt = 0;

  bool has_value() const {
    return t.has_value();
  }

  void evict() const {
    t.reset();
  }

  const T& get() const {
    assert(created_time > 0);
    if (t.has_value()) {
      return t.value();
    } else {
      Guard<T> g(this);
      auto& n = World::get_world().record.get_precise_node(created_time);
      assert(n.parent != nullptr);
      assert(n.parent->parent != nullptr);
      Computer* com = dynamic_cast<Computer*>(n.parent->value);
      assert(com != nullptr);
      com->replay(n.parent->range.first);
      ASSERT(t.has_value());
      return t.value();
    }
  }

  const void* void_ptr() const {
    return &get();
  }

  T get_value() const {
    return get();
  }

  void fill_any(Any&& a) const {
    if (!has_value()) {
      t = std::move(*static_cast<const T*>(a.void_ptr()));
    }
  }
};

struct EZombieRecord : Object {
};

template<typename X>
struct ZombieRecord : EZombieRecord {
  std::weak_ptr<ZombieNode<X>>
};

*/

// A Computer record a computation executed by bindZombie, to replay it.
// Note that a Computer may invoke more bindZombie, which may create Computer.
// When that happend, the outer Computer will not replay the inner one,
// And the metadata is measured accordingly.
// Note: Computer might be created or destroyed,
// Thus the work executed by the parent Computer will also change.
// The metadata must be updated accordingly.
struct MicroWave : Object {
  
};

struct GraveYard : Object {
  
};

template<typename F, size_t... Is>
auto gen_tuple_impl(F func, std::index_sequence<Is...> ) {
  return std::make_tuple(func(Is)...);
}

template<size_t N, typename F>
auto gen_tuple(F func) {
  return gen_tuple_impl(func, std::make_index_sequence<N>{} );
}

struct EZombieNode {
  ptrdiff_t pool_index = -1;
  virtual ~EZombieNode() { }
  virtual const void* get_ptr() const = 0;
};

template<>
struct BagObserver<std::shared_ptr<EZombieNode>> {
  void operator()(std::shared_ptr<EZombieNode>& t, size_t idx) {
    t->pool_index = idx;
  }
};

template<typename X>
struct ZombieNode : EZombieNode {
  X x;
  const void* get_ptr() const {
    return &x;
  }
  const X& get_ref() const {
    return x;
  }
  template<typename... Args>
  ZombieNode(Args&&... args) : x(std::forward<Args>(args)...) { }
};

// todo: it could be a shared_ptr to skip registering in node.
// when that happend, we gain both space and time,
// at the lose of eviction granularity.

// the shared_ptr is stored in the evict list. when it evict something it simply drop the pointer.
template<typename T>
struct Zombie {
  static_assert(!std::is_reference_v<T>, "should not be a reference");
  tock created_time;
  std::weak_ptr<ZombieNode<T>> ptr;

  bool evictable() const {
    auto ptr = this->ptr.lock();
    if (ptr) {
      return ptr->pool_index != -1;
    } else {
      return false;
    }
  }

  bool unique() const {
    return ptr.use_count() ==  1;
  }

  void evict() {
    if (evictable()) {
      auto ptr = this->ptr.lock();
      ASSERT(ptr);
      World::get_world().evict_pool.remove(ptr->pool_index);
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

  std::shared_ptr<ZombieNode<T>> shared_ptr() const {
    auto ret = ptr.lock();
    if (ret) {
      return ret;
    } else {
      throw;
    }
  }

  template<typename... Args>
  void construct(Args&&... args) {
    World& w = World::get_world();
    created_time = w.current_tock++;
    if (w.record.has_precise(created_time)) {
      throw;
      /*auto& n = w.record.get_precise_node(created_time);
        EMonster* m = dynamic_cast<EMonster*>(n.value);
        if (!m->has_value()) {
        m->fill_any(Any(T(std::forward<Args>(args)...)));
        }
        created_time *= -1;*/
    } else {
      auto shared = std::make_shared<ZombieNode<T>>(std::forward<Args>(args)...);
      ptr = shared;
      w.evict_pool.insert(shared);
      //w.record.put({created_time, created_time+1}, this);
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

  T get_value() const {
    return shared_ptr()->x;
  }
};


template<typename T>
struct IsZombie : std::false_type { };

template<typename T>
struct IsZombie<Zombie<T>> : std::true_type { };

template<typename F, typename... Arg>
auto bindZombie(F&& f, const Zombie<Arg>& ...x) {
  World& w = World::get_world();
  ScopeGuard sg(w);
  std::tuple<std::shared_ptr<ZombieNode<Arg>>...> g(x.shared_ptr()...);
  auto y = std::apply([](const std::shared_ptr<ZombieNode<Arg>>&... g_){ return std::make_tuple<>(std::cref(g_->get_ref())...); }, g);
  tock start_time = w.current_tock++;
  auto ret = std::apply(f, y);
  static_assert(IsZombie<decltype(ret)>::value, "should be zombie");
  tock end_time = w.current_tock;
  std::vector<tock> in = {x.created_time...};
  std::function<void(const std::vector<const void*>&)> func =
    [f = std::forward<F>(f)](const std::vector<const void*> in) {
      auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
      std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
      std::apply([&](const Arg*... arg){ return f(*arg...); }, args);
    };
  //w.record.put({start_time, end_time}, new Computer(std::move(func), in, ret.created_time));
  return std::move(ret);
}
