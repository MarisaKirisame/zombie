#pragma once

#include <memory>
#include <vector>
#include <functional>

#include "tock/tock.hpp"
#include "config.hpp"
#include "trampoline.hpp"

namespace ZombieInternal {

template<const ZombieConfig &cfg, typename T>
struct ExternalZombie;

template<const ZombieConfig &cfg>
struct ExternalEZombie;

template<const ZombieConfig& cfg>
struct ContextNode;

template<const ZombieConfig& cfg>
using Context = std::shared_ptr<ContextNode<cfg>>;

template<const ZombieConfig& cfg>
struct EZombie;

template<const ZombieConfig &cfg, typename T>
struct Zombie;

// a phantom type
template<const ZombieConfig &cfg, typename T>
struct TCZombie {
  TCZombie() = default;
  TCZombie(const ExternalZombie<cfg, T>&);
  TCZombie(ExternalZombie<cfg, T>&&);
};

// EZombieNode is a type-erased interface to a computed value.
// The value may be evicted later and need to be recomputed when needed again.
template<const ZombieConfig &cfg>
struct EZombieNode {
public:
  Tock created_time;
  mutable std::weak_ptr<ContextNode<cfg>> context_cache;

public:
  EZombieNode(Tock create_time);
  void accessed() const;

  virtual size_t get_size() const = 0;

  virtual ~EZombieNode() { }

  virtual const void* get_ptr() const = 0;

  std::shared_ptr<ContextNode<cfg>> get_context() const;
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

struct Phantom {
  virtual ~Phantom() {}
  virtual cost_t cost() const = 0;
  virtual void evict() = 0;
  virtual void notify_index_changed(size_t new_index) = 0;
};

template<const ZombieConfig& cfg, typename T>
struct Zombie;

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

  EZombie(const EZombie& ez) : created_time(ez.created_time), ptr_cache(ez.ptr_cache) { }
  template<typename T>
  EZombie(const Zombie<cfg, T>& ez) : created_time(ez.created_time), ptr_cache(ez.ptr_cache) { }
  EZombie() { }

  //template<typename T>
  //EZombie(ExternalZombie<cfg, T>&& z) : EZombie(std::move(z.z)) {
  //  z.a->evict = false;
  //}

  explicit EZombie(const Tock& t) : created_time(t) { }

  std::weak_ptr<EZombieNode<cfg>> ptr() const;

  bool evicted() const {
    return ptr().lock() == nullptr;
  }

  bool evictable() const {
    auto ptr = this->ptr().lock();
    if (ptr != nullptr) {
      auto ctx = ptr->get_context();
      return ctx != nullptr && ctx->evictable();
    } else {
      return false;
    }
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
// T should manage it's own memory:
// when T is destructed, only then all memory is released.
// this mean T should not hold shared_ptr, as others might be sharing it.
// T having Zombie is allowed though.
template<const ZombieConfig& cfg, typename T>
struct Zombie : EZombie<cfg> {
  static_assert(!std::is_reference_v<T>, "Zombie should not hold a reference");

  template<typename... Args>
  void construct(Args&&... args);

  explicit Zombie(const EZombie<cfg>& z) : EZombie<cfg>(z) { }
  explicit Zombie(const EZombie<cfg>&& z) : EZombie<cfg>(std::move(z)) { }
  explicit Zombie(EZombie<cfg>& z) : EZombie<cfg>(z) { }
  explicit Zombie(EZombie<cfg>&& z) : EZombie<cfg>(std::move(z)) { }

  Zombie(const Zombie<cfg, T>& z) : EZombie<cfg>(z) { }
  Zombie(const Zombie<cfg, T>&& z) : EZombie<cfg>(std::move(z)) { }
  Zombie(Zombie<cfg, T>& z) : EZombie<cfg>(z) { }
  Zombie(Zombie<cfg, T>&& z) : EZombie<cfg>(std::move(z)) { }

  explicit Zombie(const Tock& t) : EZombie<cfg>(t) { }
  explicit Zombie(const Tock&& t) : EZombie<cfg>(std::move(t)) { }
  explicit Zombie(Tock& t) : EZombie<cfg>(t) { }
  explicit Zombie(Tock&& t) : EZombie<cfg>(std::move(t)) { }

  template<typename... Arg>
  explicit Zombie(Arg&&... arg) {
    static_assert(!std::is_same<std::tuple<std::remove_cvref_t<Arg>...>, std::tuple<Zombie<cfg, T>>>::value, "should not match this constructor");
    static_assert(!std::is_same<std::tuple<std::remove_cvref_t<Arg>...>, std::tuple<TCZombie<cfg, T>>>::value, "should not match this constructor");
    static_assert(!std::is_same<std::tuple<std::remove_cvref_t<Arg>...>, std::tuple<EZombie<cfg>>>::value, "should not match this constructor");
    static_assert(!std::is_same<std::tuple<std::remove_cvref_t<Arg>...>, std::tuple<Tock>>::value, "should not match this constructor");
    static_assert(!std::is_same<std::tuple<std::remove_cvref_t<Arg>...>, std::tuple<ExternalZombie<cfg, T>>>::value, "should not match this constructor");
    static_assert(!std::is_same<std::tuple<std::remove_cvref_t<Arg>...>, std::tuple<ExternalEZombie<cfg>>>::value, "should not match this constructor");

    construct(std::forward<Arg>(arg)...);
  }

  Zombie(const T& t) {
    construct(t);
  }

  Zombie(T&& t) {
    construct(std::move(t));
  }

  Zombie() = delete;

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

template<const ZombieConfig &cfg>
struct ExternalEZombie {
  EZombie<cfg> ez;
  template<typename T>
  ExternalEZombie(ExternalZombie<cfg, T>&& z) : ez(std::move(z.z)) { }
  template<typename T>
  ExternalEZombie(const ExternalZombie<cfg, T>&& z) : ez(std::move(z.z)) { }
  template<typename T>
  ExternalEZombie(const ExternalZombie<cfg, T>& z) : ez(z.z) { }
  template<typename T>
  ExternalEZombie(ExternalZombie<cfg, T>& z) : ez(z.z) { }
  template<typename... Arg>
  explicit ExternalEZombie(Arg&&... arg) : ez(std::forward<Arg>(arg)...) { }
};

template<const ZombieConfig &cfg, typename T>
struct ExternalZombie {
  std::shared_ptr<ZombieNode<cfg, T>> shared_ptr() const {
    return z.shared_ptr();
  }
  bool evicted() const {
    return z.evicted();
  }
  Zombie<cfg, T> z;
  T get_value() const { return z.get_value(); }
  void force_unique_evict() { z.force_unique_evict(); }
  bool evictable() { return z.evictable(); }
  void evict() { z.evict(); }
  ExternalZombie(const ExternalZombie<cfg, T>& z) = default;
  ExternalZombie(ExternalZombie<cfg, T>& z) : z(z.z) { }
  ExternalZombie(ExternalZombie<cfg, T>&& z) = default;
  ExternalZombie(const ExternalZombie<cfg, T>&& z) : z(std::move(z.z)) { }
  explicit ExternalZombie(ExternalEZombie<cfg>& ez) : z(ez.ez) { }
  explicit ExternalZombie(const ExternalEZombie<cfg>& ez) : z(ez.ez) { }
  explicit ExternalZombie(ExternalEZombie<cfg>&& ez) : z(std::move(ez.ez)) { }
  explicit ExternalZombie(const ExternalEZombie<cfg>&& ez) : z(std::move(ez.ez)) { }
  template<typename... Arg>
  explicit ExternalZombie(Arg&&... arg) : z(std::forward<Arg>(arg)...) { }
};

template<typename F, size_t... Is>
auto gen_tuple_impl(F func, std::index_sequence<Is...>) {
  return std::make_tuple(func(Is)...);
}

template<size_t N, typename F>
auto gen_tuple(F func) {
  return gen_tuple_impl(func, std::make_index_sequence<N>{} );
}

template<typename T>
struct IsZombie : std::false_type { };

template<const ZombieConfig& cfg, typename T>
struct IsZombie<Zombie<cfg, T>> : std::true_type { };

template<typename T>
struct IsExternalZombie : std::false_type { };

template<const ZombieConfig& cfg, typename T>
struct IsExternalZombie<ExternalZombie<cfg, T>> : std::true_type { };

template<typename T>
struct IsTCZombie : std::false_type { };

template<const ZombieConfig& cfg, typename T>
struct IsTCZombie<TCZombie<cfg, T>> : std::true_type { };

template<typename T>
struct TCZombieInner { };

template<const ZombieConfig& cfg, typename T>
struct TCZombieInner<TCZombie<cfg, T>> { using type = T; };

} // end of namespace ZombieInternal

template<const ZombieConfig& cfg, typename T>
struct GetSize<ZombieInternal::Zombie<cfg, T>> {
  size_t operator()(const ZombieInternal::Zombie<cfg, T>& z) {
    return sizeof(z); // note that this does not care about T - it is merely 'what is the size of this struct'
  }
};

template<const ZombieConfig& cfg, typename T>
struct GetSize<ZombieInternal::ExternalZombie<cfg, T>> {
  size_t operator()(const ZombieInternal::ExternalZombie<cfg, T>& z) {
    return sizeof(z); // note that this does not care about T - it is merely 'what is the size of this struct'
  }
};
