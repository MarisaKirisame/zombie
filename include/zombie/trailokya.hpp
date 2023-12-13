#pragma once

#include <memory>

#include "tock/tock.hpp"
#include "meter.hpp"
#include "config.hpp"
#include "zombie_types.hpp"
#include "heap/gd_heap.hpp"


namespace ZombieInternal {


namespace TockTreeElemKind {
  static constexpr size_t Nothing = 0; /* should only occur at root */
  static constexpr size_t MicroWave = 1;
  static constexpr size_t ZombieNode = 2;
};

template<const ZombieConfig& cfg>
struct Trailokya {
public:
  struct Tardis {
    Tock forward_at = std::numeric_limits<Tock>::max();
    std::shared_ptr<EZombieNode<cfg>>* forward_to = nullptr;
  };


  using TockTreeElem = std::variant<
    std::monostate,
    MicroWavePtr<cfg>,
    std::shared_ptr<EZombieNode<cfg>>
  >;

  struct NotifyParentChanged {
    void operator()(const TockTreeData<TockTreeElem>& n, const TockTreeData<TockTreeElem>* parent) {
    };
  };

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
  // Hold MicroWave and GraveYard.
  TockTree<cfg.tree, TockTreeElem, NotifyParentChanged> akasha;
  GDHeap<cfg, std::unique_ptr<Phantom>, NotifyIndexChanged, NotifyElementRemoved> book;
  Tardis tardis;
  Tock current_tock = 1;
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
  std::shared_ptr<MicroWave<cfg>> get_microwave(const Tock& t) {
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
    }
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
