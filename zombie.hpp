#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <optional>
#include <iostream>
#include <map>
#include <cassert>
#include <type_traits>

#include "assert.hpp"

#include "tock.hpp"

struct Object {
  virtual ~Object() { }
};

struct EZombie : Object {
  virtual const void* void_ptr() const = 0;
  virtual void lock() const = 0;
  virtual void unlock() const = 0;
  // locked values are not evictable
  virtual bool evictable() const = 0;
  // a value may be evicted multiple time.
  // this serve as a better user-facing api,
  // because a value might be silently evict.
  virtual bool evicted() const = 0;
  virtual void evict() const = 0;
};

struct Scope {
};

struct World {
  static World& get_world() {
    static World w;
    return w;
  }

  std::vector<EZombie*> evict_pool;

  // Zombie are referenced by record while
  // Computer are held by record.
  // Zombie are removed when it is destructed while
  // Computer are removed and destructed when it has no children.
  // Note that one cannot create a Computer with no children,
  // as you have to return a Zombie, which become its children.
  tock_tree<Object*> record;

  std::vector<Scope> scopes;

  tock current_tock = 1;
};

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

// A Computer record a computation executed by bindZombie, to replay it.
// Note that a Computer may invoke more bindZombie, which may create Computer.
// When that happend, the outer Computer will not replay the inner one,
// And the metadata is measured accordingly.
// Note: Computer might be created or destroyed,
// Thus the work executed by the parent Computer will also change.
// The metadata must be updated accordingly.
struct Computer : Object {
  std::function<void(const std::vector<void*>& in)> f;
  std::vector<tock> input;
  tock output;
  size_t memory;
  int64_t compute_cost;
  Computer(std::function<void(const std::vector<void*>& in)>&& f,
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
    std::vector<Zombie*> zombie;
    std::vector<std::unique_ptr<Zombie>> temporary_zombie;
    for (const tock& t : input) {
      ASSERT(w.record.has_precise(t));

    }
    std::vector<void*> in;

    f(in);
  }
};

template<typename T>
struct Guard {
  const EZombie& ez;
  Guard(const EZombie* ezp) : ez(*ezp) {
    ez.lock();
  }
  ~Guard() {
    ez.unlock();
  }
  Guard(const Guard<T>&) = delete;
  Guard(Guard<T>&&) = delete;
  const T& get() const {
    return *static_cast<const T*>(ez.void_ptr());
  }
};

// T should manage it's own memory:
// when T is construct, only then all memory is released.
// this mean T should no contain Zombie or shared_ptr.
// however, such cases is not incorrect, it merely mess with uncompute/recompute profitability a bit.
template<typename T>
struct Zombie : EZombie {
private:
  //Zombie() : created_time(-1) { }
  Zombie<T>& operator=(Zombie<T>&& z) {
    t = std::move(z.t);
    z.t.reset();
    created_time = std::move(z.created_time);
    z.created_time = 0;
    World::get_world().record.get_node_precise(created_time).value = this;
    return *this;
  }
  Zombie<T>& operator=(const Zombie<T>&) = delete;
  template<typename F, typename... Arg>
  friend auto bindZombie(F&& f, const Zombie<Arg>&... x);
public:
  mutable std::optional<T> t;
  // 0 for moved Zombie, positive for normal, unique zombie, negative forward to corresponding positive.
  tock created_time;

  template<typename ...Args>
  void construct(Args&&... args) {
    World& w = World::get_world();
    if (w.scopes.empty()) {
      struct Constructor {
        std::tuple<std::decay_t<Args>...> args;
        Constructor(Args&&... args) : args(std::forward<Args>(args)...) { }
        Zombie<T> operator()() const {
          return std::apply([](const Args&... args){ return Zombie<T>(args...); }, args);
        }
      } con(std::forward<Args>(args)...);
      (*this) = bindZombie(std::move(con));
    } else {
      created_time = w.current_tock++;
      if (w.record.has_precise(created_time)) {
        auto& n = w.record.get_node_precise(created_time);
        Zombie<T>& z = dynamic_cast<Zombie<T>&>(*n.value);
        if (!z.t.has_value()) {
          z.t = T(std::forward<Args>(args)...);
        }
        created_time *= -1;
      } else {
        t = T(std::forward<Args>(args)...);
        w.record.put({created_time, created_time+1}, this);
      }
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

  Zombie(Zombie<T>&& z) {
    (*this) = std::move(z);
  }

  Zombie(const Zombie<T>& z) = delete;

  ~Zombie() {
    if (created_time > 0) { }
  }

  const T& unsafe_get() const {
    return  t.value();
  }

  void lock() const {
    
  }

  void unlock() const {
    
  }

  bool evictable() const {
    return true;
  }

  bool evicted() const {
    return !t.has_value();
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
      auto& n = World::get_world().record.get_node_precise(created_time);
      assert(n.parent != nullptr);
      assert(n.parent->parent != nullptr);
      Computer* com = dynamic_cast<Computer*>(n.parent->value);
      assert(com != nullptr);
      com->replay(n.parent->range.first);
      return t.value();
    }
  }

  const void* void_ptr() const {
    return &get();
  }

  T get_value() const {
    return get();
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

template<typename T>
struct IsZombie : std::false_type { };

template<typename T>
struct IsZombie<Zombie<T>> : std::true_type { };

template<typename F, typename... Arg>
auto bindZombie(F&& f, const Zombie<Arg>& ...x) {
  World& w = World::get_world();
  ScopeGuard sg(w);
  std::tuple<Guard<Arg>...> g(&x...);
  auto y = std::apply([](const Guard<Arg>&... g_){ return std::make_tuple<>(std::cref(g_.get())...); }, g);
  tock start_time = w.current_tock++;
  auto ret = std::apply(f, y);
  static_assert(IsZombie<decltype(ret)>::value, "should be zombie");
  tock end_time = w.current_tock;
  std::vector<tock> in = {x.created_time...};
  std::function<void(const std::vector<void*>&)> func =
    [f = std::forward<F>(f)](const std::vector<void*> in) {
      auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
      std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
      std::apply([&](const Arg*... arg){ return f(*arg...); }, args);
    };
  w.record.put({start_time, end_time}, new Computer(std::move(func), in, ret.created_time));
  return std::move(ret);
}
