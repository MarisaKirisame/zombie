#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <optional>
#include <iostream>
#include <map>
#include <cassert>
#include <type_traits>

#include "tock.hpp"

struct Object {
  virtual ~Object() { }
};

struct EZombie : Object {
  virtual const void* unsafe_ptr() const = 0;
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

struct EComputer : Object {
  virtual void* uncompute() = 0;
  virtual double cost() = 0;
};

struct Scope {
  tock& current_tock;
  std::vector<EZombie*> created;
  Scope(tock& current_tock) : current_tock(current_tock) { }
};

struct Context {
  std::vector<Scope> scopes;
  tock day, night;
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

  Context ctx;
};

// does not recurse
template<typename T>
struct Computer : EComputer {
  std::function<void(void* in, void* out)> f;
  std::vector<tock> input;
  std::vector<tock> output;
  size_t memory;
  int64_t compute_cost;
  time_t enter_time;
  tock started_tock;
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
    z.created_time = -1;
    return *this;
  }
  Zombie<T>& operator=(const Zombie<T>&) = delete;
public:
  mutable std::optional<T> t;
  // -1 for moved Zombie. otherwise unique.
  tock created_time;

  template<typename ...Args>
  void construct(Args&&... args) {
    World& w = World::get_world();
    if (w.ctx.scopes.empty()) {
      struct Constructor {
        std::tuple<std::decay_t<Args>...> args;
        Constructor(Args&&... args) : args(std::forward<Args>(args)...) { }
        Zombie<T> operator()() {
          return std::apply([](const Args&... args){ return Zombie<T>(args...); }, args);
        }
      } con(std::forward<Args>(args)...);
      (*this) = bindZombie(std::move(con));
    } else {
      t = T(std::forward<Args>(args)...);
      created_time = w.ctx.scopes.back().current_tock++;
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
  Zombie(Zombie<T>&& z) : t(std::move(z.t)), created_time(std::move(z.created_time)) {
    z.created_time = -1;
    z.t.reset();
  }
  Zombie(const Zombie<T>& z) = delete;
  ~Zombie() {
    if (created_time != -1) { }
  }
  const void* unsafe_ptr() const {
    return &t.value();
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
    return t.has_value();
  }
  void evict() const {
    t.reset();
  }
  T get_value() const {
    return unsafe_get();
  }
};

template<typename T>
struct Guard;

template<typename F, typename ...T>
auto bindZombieInner(bool recompute, F&& f, const Zombie<T>& ...x) {
  World& w = World::get_world();
  w.ctx.scopes.push_back(Scope(recompute ? w.ctx.night : w.ctx.day));
  std::tuple<Guard<T>...> g(&x...);
  auto y = std::apply([](const Guard<T>&... g_){ return std::make_tuple<>(std::cref(g_.get())...); }, g);
  auto ret = std::apply(f, y);
  w.ctx.scopes.pop_back();
  new F(std::forward<F>(f));
  return std::move(ret);
}
template<typename F, typename ...T>
auto bindZombie(F&& f, const Zombie<T>& ...x) {
  return bindZombieInner(false, std::forward<F>(f), x...);
}

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
    return *static_cast<const T*>(ez.unsafe_ptr());
  }
};
