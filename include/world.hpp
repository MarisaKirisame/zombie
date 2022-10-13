#pragma once

#include "tock.hpp"
#include "bag.hpp"

struct EZombieNode;
struct Object;

struct Scope {
};

struct World {
  static World& get_world() {
    static World w;
    return w;
  }

  Bag<std::shared_ptr<EZombieNode>> x;

  // Zombie are referenced by record while
  // Computer are held by record.
  // Zombie are removed when it is destructed while
  // Computer are removed and destructed when it has no children.
  // Note that one cannot create a Computer with no children,
  // as you have to return a Zombie, which become its children.
  tock_tree<Object*> record;

  std::vector<Scope> scopes;

  tock current_tock = 1;
};
