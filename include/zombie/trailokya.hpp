#pragma once

#include <memory>

#include "tock/tock.hpp"
#include "meter.hpp"
#include "config.hpp"
#include "zombie_types.hpp"



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


  struct Reaper;


public:
  // Hold MicroWave and GraveYard.
  TockTree<cfg.tree, TockTreeElem, NotifyParentChanged> akasha;
  KineticHeap<cfg.heap, std::unique_ptr<Phantom>, NotifyIndexChanged> book;
  Tardis tardis;
  Tock current_tock = 1;
  ZombieMeter meter;
  Reaper reaper = Reaper(*this);

public:
  Trailokya() : book(0) {}
  ~Trailokya() {}

  static Trailokya& get_trailokya() {
    static Trailokya t;
    return t;
  }


  // return the closest MicroWave holding [t]
  std::shared_ptr<MicroWave<cfg>> get_microwave(const Tock& t) {
    TockTreeElem elem = akasha.get_node(t).value;
    if (elem.index() == TockTreeElemKind::MicroWave)
      return std::get<TockTreeElemKind::MicroWave>(elem);

    auto parent = akasha.get_parent(t);
    if (! parent || parent->value.index() == TockTreeElemKind::Nothing)
      return nullptr;

    assert (parent->value.index() == TockTreeElemKind::MicroWave);
    return std::get<TockTreeElemKind::MicroWave>(parent->value);
  }


public:
  struct Reaper {
    Trailokya& t;

    Reaper(Trailokya& t) : t(t) { }

    bool have_soul() {
      return !t.book.empty();
    }

    void advance() {
      t.book.advance_to(Time(t.meter.time()).count());
    }

    void murder() {
      advance();

      assert (t.book.size() > 0);

      AffFunction old_aff = t.book.get_aff(t.book.min_idx());
      auto phantom = t.book.pop();
      AffFunction new_aff = phantom->get_aff();
      auto old_val = old_aff(t.book.time());
      auto new_val = new_aff(t.book.time());
      if (new_val < 0) {
        new_val = - new_val;
        old_val = - old_val;
      }
      if (old_val / cfg.approx_factor.first <= new_val / cfg.approx_factor.second
       && new_val / cfg.approx_factor.first <= old_val / cfg.approx_factor.second)
        phantom->evict();
      else {
        t.book.push(std::move(phantom), new_aff);
        murder();
      }
    }

    aff_t score() {
      advance();
      return t.book.get_aff(t.book.min_idx())(t.book.time());
    }

    void mass_extinction(aff_t threshold) {
      if (have_soul()) {
        std::cout << "Score: " << score() << std::endl;
      }
      while(have_soul() && score() < threshold) {
        puts("Murder!");
        murder();
      }
    }
  };
};

} // end of namespace ZombieInternal
