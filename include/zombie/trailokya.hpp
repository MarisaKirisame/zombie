#pragma once

#include <memory>

#include "tock.hpp"
#include "kinetic.hpp"
#include "base.hpp"

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


  Trailokya() : book(0) {}

  static Trailokya& get_trailokya() {
    static Trailokya t;
    return t;
  }
};
