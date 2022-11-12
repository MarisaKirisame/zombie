#pragma once

#include "tock.hpp"
#include "bag.hpp"
#include "base.hpp"

struct EZombieNode;

struct Scope {
};

struct Trailokya {
  static Trailokya& get_trailokya() {
    static Trailokya t;
    return t;
  }
  bool in_ragnarok = false;
  ~Trailokya() {
    in_ragnarok = true;
  }
  Bag<std::shared_ptr<EZombieNode>> book;
  // Hold MicroWave and GraveYard.
  tock_tree<std::unique_ptr<Object>> akasha;
  std::vector<Scope> scopes;
  Tock current_tock = 1;
};
