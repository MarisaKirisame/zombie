#pragma once

#include "tock.hpp"
#include "bag.hpp"
#include "base.hpp"

struct EZombieNode;

struct Tardis {
  Tock forward_at = std::numeric_limits<Tock>::max();
  std::shared_ptr<EZombieNode>* forward_to = nullptr;
};

struct Phantom {
  virtual ~Phantom() { }
  virtual void evict() = 0;
  virtual void notify_bag_index_changed(size_t new_index) = 0;
};

struct Trailokya {
  static Trailokya& get_trailokya() {
    static Trailokya t;
    return t;
  }
  // Hold MicroWave and GraveYard.
  tock_tree<std::unique_ptr<Object>> akasha;
  Bag<std::unique_ptr<Phantom>> book;
  Tardis tardis;
  Tock current_tock = 1;
};