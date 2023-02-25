#pragma once

#include "tock.hpp"
#include "bag.hpp"
#include "base.hpp"

struct EZombieNode;

struct Tardis {
  Tock forward_at = std::numeric_limits<Tock>::max();
  std::shared_ptr<EZombieNode>* forward_to = nullptr;
};

struct Trailokya {
  static Trailokya& get_trailokya() {
    static Trailokya t;
    return t;
  }
  // i have double free without this. figure out why.
  bool in_ragnarok = false;
  ~Trailokya() {
    in_ragnarok = true;
  }
  // Hold MicroWave and GraveYard.
  tock_tree<std::unique_ptr<Object>> akasha;
  // Note that book goes after akasha, as destruction of book access akasha
  Bag<std::shared_ptr<EZombieNode>> book;
  Tardis tardis;
  Tock current_tock = 1;
};
