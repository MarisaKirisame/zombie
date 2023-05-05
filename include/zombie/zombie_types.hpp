#pragma once

#include <memory>
#include <vector>
#include <functional>

#include "tock/tock.hpp"
#include "config.hpp"


namespace ZombieInternal {

// A MicroWave record a computation executed by bindZombie, to replay it.
// Note that a MicroWave may invoke more bindZombie, which may create MicroWave.
// When that happend, the outer MicroWave will not replay the inner one,
// And the metadata is measured accordingly.
// Note: MicroWave might be created or destroyed,
// Thus the work executed by the parent MicroWave will also change.
// The metadata must be updated accordingly.
template<const ZombieConfig& cfg>
struct MicroWave {
public:
  // we dont really use the Tock return type, but this allow one less boxing.
  std::function<Tock(const std::vector<const void*>& in)> f;
  std::vector<Tock> inputs;
  Tock output;
  Tock start_time;
  Tock end_time;

  Time time_taken;
  Space space_taken;

public:
  MicroWave(std::function<Tock(const std::vector<const void*>& in)>&& f,
            const std::vector<Tock>& inputs,
            const Tock& output,
            const Tock& start_time,
            const Tock& end_time,
            const Space& space,
            const Time& time_taken):
    f(std::move(f)),
    inputs(inputs),
    output(output),
    start_time(start_time),
    end_time(end_time),
    space_taken(space),
    time_taken(time_taken) { }

  static Tock play(const std::function<Tock(const std::vector<const void*>& in)>& f,
                   const std::vector<Tock>& inputs);
  void replay();
};




// EZombieNode is a type-erased interface to a computed value.
// The value may be evicted later and need to be recomputed when needed again.
template<const ZombieConfig &cfg>
struct EZombieNode {
public:
  Tock created_time;
  ptrdiff_t pool_index = -1;
  mutable Time last_accessed;

public:
  EZombieNode(Tock create_time);
  void accessed() const;

  virtual size_t get_size() const = 0;

  virtual ~EZombieNode() { }

  virtual const void* get_ptr() const = 0;
};


// ZombieNode is the concrete implementation of EZombieNode,
template<const ZombieConfig &cfg, typename T>
struct ZombieNode : EZombieNode<cfg> {
  T t;

  size_t get_size() const override {
    return GetSize<T>()(t);
  }

  const void* get_ptr() const override {
    EZombieNode<cfg>::accessed();
    return &t;
  }

  const T& get_ref() const {
    EZombieNode<cfg>::accessed();
    return t;
  }

  ZombieNode(ZombieNode<cfg, T>&& t) = delete;

  template<typename... Args>
  ZombieNode(Tock created_time, Args&&... args);
};




class Phantom {
public:
  virtual ~Phantom() {}
  virtual void evict() = 0;
  virtual void notify_index_changed(size_t new_index) = 0;
};

// RecomputeLater wraps holds a weak pointer to a EZombieNode,
// and is stored in Trailokya::book for eviction.
template<const ZombieConfig& cfg>
struct RecomputeLater : Phantom {
  Tock created_time;
  std::weak_ptr<EZombieNode<cfg>> weak_ptr;

  RecomputeLater(const Tock& created_time, const std::shared_ptr<EZombieNode<cfg>>& ptr) : created_time(created_time), weak_ptr(ptr) { }

  void evict() override;
  void notify_index_changed(size_t idx) override {
      non_null(weak_ptr.lock())->pool_index = idx;
  }
};



// Note that this type do not have a virtual destructor.
// Doing so save the pointer to the virtual method table,
//   and a EZombie only contain two 64 bit field:
//   created_time and ptr_cache.
// As a consequence, Zombie only provide better API:
//   it cannot extend EZombie in any way.
template<const ZombieConfig& cfg>
struct EZombie {
  Tock created_time;
  mutable std::weak_ptr<EZombieNode<cfg>> ptr_cache;

  EZombie(Tock created_time) : created_time(created_time) { }
  EZombie() { }

  std::weak_ptr<EZombieNode<cfg>> ptr() const;

  bool evicted() const {
    return ptr().lock() == nullptr;
  }
  bool evictable() const {
    auto ptr = this->ptr().lock();
    return ptr && ptr->pool_index != -1;
  }
  bool unique() const {
    return ptr().use_count() ==  1;
  }

  void evict();

  void force_evict() {
    assert(evictable());
    evict();
  }

  void force_unique_evict() {
    assert(evictable());
    assert(unique());
    evict();
  }


  std::shared_ptr<EZombieNode<cfg>> shared_ptr() const;
};



// The core of the library.
// Represent something that could be recomputed to save space.
// While Zombie is mutable, the inside is immutable.
// Note that when you have a Zombie of Zombie, the inside is immutable by the above rule.
//
// TODO: it could be a shared_ptr to skip registering in node.
// when that happend, we gain both space and time,
// at the lose of eviction granularity.
//
// the shared_ptr is stored in the evict list. when it evict something it simply drop the pointer.
// T should manage it's own memory:
// when T is construct, only then all memory is released.
// this mean T should not hold shared_ptr.
// T having Zombie is allowed though.
template<const ZombieConfig& cfg, typename T>
struct Zombie : EZombie<cfg> {
  static_assert(!std::is_reference_v<T>, "should not be a reference");

  template<typename... Args>
  void construct(Args&&... args);

  Zombie(const Zombie<cfg, T>& z) : EZombie<cfg>(z) { }
  Zombie(Zombie<cfg, T>& z) : EZombie<cfg>(z) { }
  Zombie(Zombie<cfg, T>&& z) : EZombie<cfg>(std::move(z)) { }
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
  Zombie(Tock&& t) : EZombie<cfg>(t) { }
  Zombie(const Tock& t) : EZombie<cfg>(t) { }
  Zombie(Tock& t) : EZombie<cfg>(t) { }

  std::shared_ptr<ZombieNode<cfg, T>> shared_ptr() const {
    return non_null(std::dynamic_pointer_cast<ZombieNode<cfg, T>>(EZombie<cfg>::shared_ptr()));
  }
  void recompute() const {
    shared_ptr();
  }
  T get_value() const {
    return shared_ptr()->get_ref();
  }
};

} // end of namespace ZombieInternal
