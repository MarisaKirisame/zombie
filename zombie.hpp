#pragma once

#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>
#include <optional>
#include <iostream>

struct EZombieNode {
  virtual ~EZombieNode() { }
  virtual void* unsafe_ptr() = 0;
  virtual void lock() = 0;
  virtual void unlock() = 0;
};

using EZombie = std::shared_ptr<EZombieNode>;
using WEZombie = std::weak_ptr<EZombieNode>;

struct EComputerNode {
  virtual ~EComputerNode() { }
  virtual void* uncompute() = 0;
  virtual double cost() = 0;
};

using EComputer = std::shared_ptr<EComputerNode>;
using WEComputer = std::weak_ptr<EComputerNode>;

// start at 0.
// a tock pass whenever a Computer start execution, or a Zombie is created.
// we denote the start and end(open-close) of Computer execution as a tock_range.
// likewise, Zombie also has a tock_range with exactly 1 tock in it.
// One property of all the tock_range is, there is no overlapping.
// A tock_range might either be included by another tock_range,
// include that tock_range, or have no intersection.
// This allow a bunch of tock_range in a tree.
// From a tock, we can regain a Computer (to rerun function),
// or a Zombie, to reuse values.
// We use tock instead of e.g. pointer for indexing,
// because doing so allow a far more compact representation,
// and we use a single index instead of separate index, because Computer and Zombie relate -
// the outer tock_range node can does everything an inner tock_range node does,
// so one can replay that instead if the more fine grained option is absent.
using tock_t = int64_t;

struct Record {
  tock_t end_tock;
  WEZombie value;
};

struct Scope {
  tock_t &tock;
  std::vector<WEZombie> created;
};

struct Context {
  std::vector<Scope> scopes;
  tock_t day, night;
};

struct World {
  static World& get_world();
  // maybe this should be an kinetic datastructure, we will see
  std::vector<WEComputer> evict_pool;
  // when replaying a function, look at recorded to repatch recursive remat, thus existing fragment can be reused
  std::unordered_map<tock_t, Record> recorded;

  // recorded at above is not too correct... I think it should be more like a tree.

  Context ctx;
};

// does not recurse
template<typename T>
struct ComputerNode : EComputerNode {
  std::function<void(void* in, void* out)> f;
  std::vector<EZombie> input;
  std::vector<WEZombie> output;
  size_t memory;
  int64_t compute_cost;
  time_t enter_time;
  tock_t started_tock;
};

template<typename T>
struct Computer {
  std::shared_ptr<ComputerNode<T>> node;
};

template<typename T>
struct ZombieNode : EZombieNode {
  std::optional<T> t;
  // multiple ZombieNode may share the same created_time
  tock_t created_time;
  ZombieNode(const T& t) : t(t) { }
  void* unsafe_ptr() {
    return &t.value();
  }
  const T& get() {
   return  t.value();
  }
  void lock() {
    
  }
  void unlock() {
    
  }
};

template<typename T>
struct Zombie {
  std::shared_ptr<ZombieNode<T>> node;
};

template<typename T>
struct Guard;

template<typename T>
Zombie<T> mkZombie(const T& t) {
  return { std::make_shared<ZombieNode<T>>(t) };
}

template<typename F, typename ...T>
auto bindZombie(const F& f, const Zombie<T>& ...x) {
  std::tuple<Guard<T>...> g(x.node.get()...);
  auto y = std::apply([](const Guard<T>&... g_){ return std::make_tuple<>(std::cref(g_.get())...); }, g);
  return std::apply(f, y);
}

template<typename T>
struct Guard {
  EZombieNode& ezn;
  Guard(EZombieNode* eznp) : ezn(*eznp) {
    ezn.lock();
  }
  ~Guard() {
    ezn.unlock();
  }
  Guard(const Guard<T>&) = delete;
  Guard(Guard<T>&&) = delete;
  const T& get() const {
    return *static_cast<const T*>(ezn.unsafe_ptr());
  }
};
