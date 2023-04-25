#pragma once

#include <memory>

#include "tock/tock.hpp"
#include "time.hpp"
#include "config.hpp"
#include "zombie_types.hpp"



namespace ZombieInternal {


class TockTreeElemKind {
public:
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
    MicroWave<cfg>,
    std::shared_ptr<EZombieNode<cfg>>
  >;

  struct NotifyParentChanged {
    void operator()(const TockTreeData<TockTreeElem>& n, const TockTreeData<TockTreeElem>* parent) {
      if (n.value.index() == TockTreeElemKind::ZombieNode) {
        std::shared_ptr<EZombieNode<cfg>> ptr = std::get<TockTreeElemKind::ZombieNode>(n.value);
        if (parent != nullptr && parent->value.index() == TockTreeElemKind::MicroWave) {
          const MicroWave<cfg> &pobj = std::get<TockTreeElemKind::MicroWave>(parent->value);
          assert(n.range.beg + 1 == n.range.end);
          AffFunction aff = cfg.metric(ptr->last_accessed, pobj.time_taken, ptr->get_size());
          Trailokya<cfg>::get_trailokya().book.push(std::make_unique<RecomputeLater<cfg>>(n.range.beg, ptr), std::move(aff));
        }
      }
    };
  };

  struct NotifyIndexChanged {
    void operator()(const std::unique_ptr<Phantom>& p, size_t idx) {
      p->notify_index_changed(idx);
    };
  };


  struct Reaper;


public:
  // Hold MicroWave and GraveYard.
  TockTree<cfg.tree, TockTreeElem, NotifyParentChanged> akasha;
  KineticHeap<cfg.heap, std::unique_ptr<Phantom>, NotifyIndexChanged> book;
  Tardis tardis;
  Tock current_tock = 1;
  ZombieClock zc;
  Reaper reaper = Reaper(*this);

public:
  Trailokya() : book(0) {}
  ~Trailokya() {}

  static Trailokya& get_trailokya() {
    static Trailokya t;
    return t;
  }


public:
  struct Reaper {
    Trailokya& t;

    Reaper(Trailokya& t) : t(t) { }

    bool have_soul() {
      return t.book.empty();
    }

    void advance() {
      t.book.advance_to(Time(t.zc.time()).count());
    }

    void murder() {
      advance();
      t.book.pop()->evict();
    }

    aff_t score() {
      advance();
      return t.book.get_aff(t.book.min_idx())(t.book.time());
    }

    void mass_extinction(aff_t threshold) {
      while(have_soul() && score() < threshold) {
        murder();
      }
    }
  };
};

}
