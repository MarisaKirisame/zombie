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

struct Trailokya {
  // Hold MicroWave and GraveYard.
  tock_tree<std::unique_ptr<Object>> akasha;
  KineticMinHeap<std::unique_ptr<Phantom>, hanger> book;
  Tardis tardis;
  Tock current_tock = 1;
  ZombieClock zc;

  Trailokya() : book(0) {}

  static Trailokya& get_trailokya() {
    static Trailokya t;
    return t;
  }
};

struct Reaper {
  Trailokya& t = Trailokya::get_trailokya();

  bool have_soul() {
    return t.book.empty();
  }

  void advance() {
    t.book.advance_to(Time(t.zc.time()).count());
  }

  void evict_one() {
    advance();
    t.book.pop();
  }

  aff_t score() {
    advance();
    return t.book.get_aff(t.book.min_idx())(t.book.time());
  }
};
