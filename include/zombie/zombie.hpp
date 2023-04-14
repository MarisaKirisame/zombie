#pragma once

#include <memory>
#include <functional>
#include <vector>

#include "tock.hpp"
#include "trailokya.hpp"
#include "common.hpp"

// A MicroWave record a computation executed by bindZombie, to replay it.
// Note that a MicroWave may invoke more bindZombie, which may create MicroWave.
// When that happend, the outer MicroWave will not replay the inner one,
// And the metadata is measured accordingly.
// Note: MicroWave might be created or destroyed,
// Thus the work executed by the parent MicroWave will also change.
// The metadata must be updated accordingly.
struct MicroWave : Object {
  // we dont really use the Tock return type, but this allow one less boxing.
  std::function<Tock(const std::vector<const void*>& in)> f;
  std::vector<Tock> inputs;
  Tock output;
  Tock start_time;
  Tock end_time;

  MicroWave(std::function<Tock(const std::vector<const void*>& in)>&& f,
            const std::vector<Tock>& inputs,
            const Tock& output,
            const Tock& start_time,
            const Tock& end_time) :
    f(std::move(f)),
    inputs(inputs),
    output(output),
    start_time(start_time),
    end_time(end_time) { }

  static Tock play(const std::function<Tock(const std::vector<const void*>& in)>& f,
                   const std::vector<Tock>& inputs);
  void replay();
};


// EZombieNode is a type-erased interface to a computed value.
// The value may be evicted later and need to be recomputed when needed again.
struct EZombieNode : Object {
  Tock created_time;
  ptrdiff_t pool_index = -1;

  EZombieNode(Tock created_time) : created_time(created_time) { }
  virtual ~EZombieNode() { }

  virtual const void* get_ptr() const = 0;
};



// ZombieNode is the concrete implementation of EZombieNode,
template<typename T>
struct ZombieNode : EZombieNode {
  T t;

  const void* get_ptr() const override {
    return &t;
  }
  const T& get_ref() const {
    return t;
  }

  ZombieNode(ZombieNode<T>&& t) = delete;

  template<typename... Args>
  ZombieNode(Tock created_time, Args&&... args) : EZombieNode(created_time), t(std::forward<Args>(args)...) { }
};


// RecomputeLater wraps holds a weak pointer to a EZombieNode,
// and is stored in Trailokya::book for eviction.
struct RecomputeLater : Phantom {
  Tock created_time;
  std::weak_ptr<EZombieNode> weak_ptr;

  RecomputeLater(const Tock& created_time, const std::shared_ptr<EZombieNode>& ptr) : created_time(created_time), weak_ptr(ptr) { }

  void evict() override {
    auto& t = Trailokya::get_trailokya();
    t.akasha.get_precise_node(created_time).delete_node();
  }

  void notify_bag_index_changed(size_t idx) override {
      non_null(weak_ptr.lock())->pool_index = idx;
  }
};


template<>
struct NotifyBagIndexChanged<std::unique_ptr<Phantom>> {
  void operator()(const std::unique_ptr<Phantom>& p, size_t idx) {
    non_null(p)->notify_bag_index_changed(idx);
  }
};



// GraveYard wraps around a shared pointer to EZombieNode,
// and should be stored in a tock tree (Trailokya::akasha).
struct GraveYard : Object {
  std::shared_ptr<EZombieNode> ptr;
  explicit GraveYard(const std::shared_ptr<EZombieNode>& ptr) : ptr(ptr) { }
};


// Trailokya::akasha should hold only two kinds of value:
// 1. MicroWave
// 2. shared_ptr<EZombieNode>, wrapped in GraveYard
template<>
struct NotifyParentChanged<std::unique_ptr<Object>> {
  void operator()(const typename tock_tree<std::unique_ptr<Object>>::Node& n) {
    Object* obj = n.value.get();
    if (GraveYard* gy = dynamic_cast<GraveYard*>(obj)) {
      if (n.parent != nullptr && n.parent->parent != nullptr) {
        tock_range tr = n.range;
        assert(tr.first + 1 == tr.second);
        Trailokya::get_trailokya().book.insert(std::make_unique<RecomputeLater>(tr.first, gy->ptr));
      }
    } else
        assert (dynamic_cast<MicroWave*>(obj));
  }
};




// Note that this type do not have a virtual destructor.
// Doing so save the pointer to the virtual method table,
//   and a EZombie only contain two 64 bit field:
//   created_time and ptr_cache.
// As a consequence, Zombie only provide better API:
//   it cannot extend EZombie in any way.
struct EZombie {
  Tock created_time;
  mutable std::weak_ptr<EZombieNode> ptr_cache;

  EZombie(Tock created_time) : created_time(created_time) { }
  EZombie() { }

  std::weak_ptr<EZombieNode> ptr() const {
    if (ptr_cache.expired()) {
      Trailokya& t = Trailokya::get_trailokya();
      if (t.akasha.has_precise(created_time)) {
        GraveYard* gy = non_null(dynamic_cast<GraveYard*>(t.akasha.get_precise_node(created_time).value.get()));
        ptr_cache = non_null(gy->ptr);
      }
    }
    return ptr_cache;
  }

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

  void evict() {
    if (evictable()) {
      auto ptr = non_null(this->ptr().lock());
      Trailokya::get_trailokya().book.remove(ptr->pool_index)->evict();
    }
  }
  void force_evict() {
    assert(evictable());
    evict();
  }
  void force_unique_evict() {
    assert(evictable());
    assert(unique());
    evict();
  }

  std::shared_ptr<EZombieNode> shared_ptr() const {
    auto ret = ptr().lock();
    if (ret) {
      return ret;
    } else {
      auto& t = Trailokya::get_trailokya();
      if (!t.akasha.has_precise(created_time)) {
        std::shared_ptr<EZombieNode> strong;
        Tardis tardis = t.tardis;
        bracket([&]() { t.tardis = Tardis { created_time, &strong }; },
                [&]() { dynamic_cast<MicroWave*>(t.akasha.get_node(created_time).value.get())->replay(); },
                [&]() { t.tardis = tardis; });
        ret = non_null(strong);
      } else {
        auto& n = t.akasha.get_precise_node(created_time);
        GraveYard* gy = non_null(dynamic_cast<GraveYard*>(n.value.get()));
        ret = non_null(gy->ptr);
      }
      ptr_cache = ret;
      return ret;
    }
  }


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
template<typename T>
struct Zombie : EZombie {
  static_assert(!std::is_reference_v<T>, "should not be a reference");

  template<typename... Args>
  void construct(Args&&... args) {
    Trailokya& t = Trailokya::get_trailokya();
    created_time = t.current_tock++;
    if (!t.akasha.has_precise(created_time)) {
     auto shared = std::make_shared<ZombieNode<T>>(created_time, std::forward<Args>(args)...);
      ptr_cache = shared;
      t.akasha.put({created_time, created_time + 1}, std::make_unique<GraveYard>(shared));
    }
    if (t.tardis.forward_at == created_time) {
      *t.tardis.forward_to = shared_ptr();
    }
  }

  Zombie(const Zombie<T>& z) : EZombie(z) { }
  Zombie(Zombie<T>& z) : EZombie(z) { }
  Zombie(Zombie<T>&& z) : EZombie(std::move(z)) { }
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
  Zombie(Tock&& t) : EZombie(t) { }
  Zombie(const Tock& t) : EZombie(t) { }
  Zombie(Tock& t) : EZombie(t) { }

  std::shared_ptr<ZombieNode<T>> shared_ptr() const {
    return non_null(std::dynamic_pointer_cast<ZombieNode<T>>(EZombie::shared_ptr()));
  }
  void recompute() const {
    shared_ptr();
  }
  T get_value() const {
    return shared_ptr()->t;
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

template<typename ret_type>
ret_type bindZombieRaw(std::function<Tock(const std::vector<const void*>&)>&& func, std::vector<Tock>&& in) {
  static_assert(IsZombie<ret_type>::value, "should be zombie");
  Trailokya& t = Trailokya::get_trailokya();
  if (!t.akasha.has_precise(t.current_tock)) {
    Tock start_time = t.current_tock++;
    Tock out = MicroWave::play(func, in);
    Tock end_time = t.current_tock;
    ret_type ret(out);
    t.akasha.put({start_time, end_time}, std::make_unique<MicroWave>(std::move(func), in, out, start_time, end_time));
    return ret;
  } else {
    auto& n = t.akasha.get_precise_node(t.current_tock);
    t.current_tock = n.range.second;
    static_assert(IsZombie<ret_type>::value, "should be zombie");
    MicroWave* mv = non_null(dynamic_cast<MicroWave*>(n.value.get()));
    ret_type ret(mv->output);
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

template<typename F, typename... Arg>
auto bindZombie(F&& f, const Zombie<Arg>& ...x) {
  using ret_type = decltype(f(std::declval<Arg>()...));
  std::function<Tock(const std::vector<const void*>&)> func =
    [f = std::forward<F>(f)](const std::vector<const void*> in) {
      auto in_t = gen_tuple<sizeof...(Arg)>([&](size_t i) { return in[i]; });
      std::tuple<const Arg*...> args = std::apply([](auto... v) { return std::make_tuple<>(static_cast<const Arg*>(v)...); }, in_t);
      auto z = std::apply([&](const Arg*... arg) { return f(*arg...); }, args);
      return z.created_time;
    };
  std::vector<Tock> in = {x.created_time...};
  return bindZombieRaw<ret_type>(std::move(func), std::move(in));
}

// While this function is not strictly necessary, as it could be replaced by binding and passing a Zombie<list<Any>>,
// doing so is clumsy to use and inefficient.
template<typename F>
auto bindZombieUnTyped(F&& f, const std::vector<EZombie>& x) {
  using ret_type = decltype(f(std::declval<std::vector<const void*>>()));
  std::vector<Tock> in;
  for (const EZombie& ez : x) {
    in.push_back(ez.created_time);
  }
  std::function<Tock(const std::vector<const void*>&)> func =
    [f = std::forward<F>(f)](const std::vector<const void*> in) {
      auto z = f(in);
      return z.created_time;
    };
  return bindZombieRaw<ret_type>(std::move(func), std::move(in));
}

// exist only to assert that linking is ok
void zombie_link_test();
