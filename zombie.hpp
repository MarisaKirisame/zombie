#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <optional>
#include <iostream>
#include <map>
#include <cassert>

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
using tock = int64_t;

// open-close.
using tock_range = std::pair<tock, tock>;

template<typename V>
struct tock_tree {
  struct Node {
    // nullptr iff toplevel
    Node* parent;
    tock_range range;
    V value;
    Node(Node* parent, const tock_range& range, const V& value) :
      parent(parent), range(range), value(value) { }
    std::map<tock, Node> children;
  };
  std::map<tock, Node> children;
  static bool static_in_range(const std::map<tock, Node>& children, const tock& t) {
    auto it = children.upper_bound(t);
    if (it == children.end()) {
      return false;
    } else {
      assert(it->second.range.first <= t);
      return t < it->second.range.second;
    }
  }
  bool in_range(const tock& t) const {
    return static_in_range(children, t);
  }
  static const Node& static_get_shallow(const std::map<tock, Node>& children, const tock& t) {
    auto it = children.upper_bound(t);
    assert(it != children.end());
    assert(it->second.range.first <= t);
    assert(t < it->second.range.second);
    return it->second;
  }
  static Node& static_get_shallow(std::map<tock, Node>& children, const tock& t) {
    auto it = children.upper_bound(t);
    assert(it != children.end());
    assert(it->second.range.first <= t);
    assert(t < it->second.range.second);
    return it->second;
  }
  Node& get_node(const tock& t) {
    Node* ptr = &static_get_shallow(children, t);
    while (static_in_range(ptr->children, t)) {
      ptr = &static_get_shallow(ptr->children, t);
    }
    return *ptr;
  }
  const Node& get_node(const tock& t) const {
    const Node* ptr = &static_get_shallow(children, t);
    while (static_in_range(ptr->children, t)) {
      ptr = &static_get_shallow(ptr->children, t);
    }
    return *ptr;
  }
  // get the most precise range that contain t
  V get(const tock& t) {
    return get_node(t).value;
  }
  void delete_node(Node& node) {
    if (node.parent == nullptr) {
      children.erase(node.range.first);
    } else {
      std::map<tock, Node>& insert_to = node.parent->children;
      for (auto it = node.children.begin(); it != node.children.end();) {
        auto next_it = it;
        ++next_it;
        auto nh = node.children.extract(it);
        nh.mapped().parent = node.parent->parent;
        insert_to.insert(std::move(nh));
        it = next_it;
      }
      node.parent->children.erase(node.range.first);
    }
  }
  void put(const tock_range& r, const V& v) {
    if (in_range(r.first)) {
      auto n = get_node(r.first);
      n.children.insert({r.first, Node(&n, r, v)});
    } else {
      children.insert({r.first, Node(nullptr, r, v)});
    }
  }
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
