#pragma once

#include <memory>

#include "tock.hpp"
#include "kinetic.hpp"
#include "base.hpp"
#include "time.hpp"

struct EZombieNode;

struct Tardis {
  Tock forward_at = std::numeric_limits<Tock>::max();
  std::shared_ptr<EZombieNode>* forward_to = nullptr;
};

struct Phantom {
  virtual ~Phantom() { }
  virtual void evict() = 0;
  virtual void notify_index_changed(size_t new_index) = 0;
};

const constexpr bool hanger = true;

struct Reaper;

struct Trailokya {
  // Hold MicroWave and GraveYard.
  tock_tree<std::unique_ptr<Object>> akasha;
  KineticMinHeap<std::unique_ptr<Phantom>, hanger> book;
  Tardis tardis;
  Tock current_tock = 1;
  ZombieClock zc;
  std::unique_ptr<Reaper> reaper = std::make_unique<Reaper>(*this);

  Trailokya();
  ~Trailokya();

  static Trailokya& get_trailokya() {
    static Trailokya t;
    return t;
  }
};

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
