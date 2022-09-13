#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <optional>
#include <iostream>
#include <map>
#include <cassert>

#include "tock.hpp"

struct Object {
  virtual ~Object() { }
};

struct EZombie : Object {
  virtual const void* unsafe_ptr() const = 0;
  virtual void lock() const = 0;
  virtual void unlock() const = 0;
};

struct EComputer : Object {
  virtual void* uncompute() = 0;
  virtual double cost() = 0;
};

struct Scope {
  tock &current_tock;
  std::vector<EZombie*> created;
};

struct Context {
  std::vector<Scope> scopes;
  tock day, night;
};

struct World {
  static World& get_world();

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

template<typename T>
struct Zombie : EZombie {
  std::optional<T> t;
  // multiple ZombieNode may share the same created_time
  tock created_time;
  Zombie(const T& t) : t(t) { }
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
};

template<typename T>
struct Guard;

template<typename F, typename ...T>
auto bindZombie(const F& f, const Zombie<T>& ...x) {
  std::tuple<Guard<T>...> g(&x...);
  auto y = std::apply([](const Guard<T>&... g_){ return std::make_tuple<>(std::cref(g_.get())...); }, g);
  return std::apply(f, y);
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
