#pragma once

#include <memory>

#include "tock/tock.hpp"
#include "meter.hpp"
#include "config.hpp"
#include "zombie_types.hpp"
#include "heap/gd_heap.hpp"

namespace ZombieInternal {

template<typename T>
struct UF {
  std::shared_ptr<UF> parent;
  T t; // only meaningful when parent.get() == nullptr
};

struct RootExclusivePreContext { };
struct HeadExclusivePreContext {
  // we dont really use the Tock return type, but this allow one less boxing.
  std::function<Trampoline::Output<Tock>(const std::vector<const void*>& in)> f;
  std::vector<Tock> inputs;

  Space space_taken;
  ns start_time;

  HeadExclusivePreContext(std::function<Trampoline::Output<Tock>(const std::vector<const void*>& in)>&& f, std::vector<Tock>&& inputs);
};
struct SpineExclusivePreContext {
  Tock head_t;
  std::vector<Tock> inputs;

  Space space_taken;
  ns start_time;
};

struct Context {
  
};

template<const ZombieConfig& cfg>
Tock current_tock();

template<const ZombieConfig& cfg>
struct Record {
  std::variant<RootExclusivePreContext, HeadExclusivePreContext, SpineExclusivePreContext> pre_context;
  Tock t;
  std::vector<std::shared_ptr<EZombieNode<cfg>>> ez;

  template<typename X>
  Record(X&& pc) : pre_context(std::forward<X>(pc)), t(current_tock<cfg>()) { }

  template<typename X>
  Record(X&& pc, Tock t) : pre_context(std::forward<X>(pc)), t(t) { }

  Record<cfg> finish() {
    assert(false);
  };
};

template<const ZombieConfig& cfg>
struct Replay {
  Tock forward_at = std::numeric_limits<Tock>::max();
  std::shared_ptr<EZombieNode<cfg>>* forward_to = nullptr;
};

template<const ZombieConfig& cfg>
struct Trailokya {
public:
  struct NotifyIndexChanged {
    void operator()(const std::unique_ptr<Phantom>& p, size_t idx) {
      p->notify_index_changed(idx);
    };
  };

  struct NotifyElementRemoved {
    void operator()(const std::unique_ptr<Phantom>&) { }
  };

  struct Reaper;
public:
  Tock current_tock = 1;
  SplayList<Tock, Context> akasha;
  GDHeap<cfg, std::unique_ptr<Phantom>, NotifyIndexChanged, NotifyElementRemoved> book;
  Record<cfg> record = Record<cfg>(RootExclusivePreContext(), 0);
  Replay<cfg> replay;
  ZombieMeter meter;
  Reaper reaper = Reaper(*this);

public:
  Trailokya() { }
  ~Trailokya() { }

  static Trailokya& get_trailokya() {
    static Trailokya t;
    return t;
  }

  // return the closest MicroWave holding [t]
  std::shared_ptr<MicroWave<cfg>> get_microwave(const Tock& t) {/*
    TockTreeElem elem = akasha.get_node(t).value;
    if (elem.index() == TockTreeElemKind::MicroWave) {
      return std::get<TockTreeElemKind::MicroWave>(elem);
    } else {
      auto parent = akasha.get_parent(t);
      if (! parent || parent->value.index() == TockTreeElemKind::Nothing) {
        return nullptr;
      } else {
        assert (parent->value.index() == TockTreeElemKind::MicroWave);
        return std::get<TockTreeElemKind::MicroWave>(parent->value);
      }
      }*/
    assert(false);
  }

public:
  struct Reaper {
    Trailokya& t;

    Reaper(Trailokya& t) : t(t) { }

    bool have_soul() {
      return t.book.empty();
    }

    void murder() {
      assert (t.book.size() > 0);
      t.book.adjust_pop([](const std::unique_ptr<Phantom>& p) { return p->cost(); })->evict();
    }

    uint64_t score() {
      return t.book.score();
    }
  };
};

} // end of namespace ZombieInternal
