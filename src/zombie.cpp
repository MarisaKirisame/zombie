#include <iostream>
#include "Zombie/zombie.hpp"

void MicroWave::replay() {
  Trailokya& t = Trailokya::get_trailokya();
  struct Tardis {
    Trailokya& t;
    tock old_tock;
    Tardis(Trailokya& t, const tock& new_tock) : t(t), old_tock(t.current_tock) {
      t.current_tock = new_tock;
    }
    ~Tardis() {
      t.current_tock = old_tock;
    }
  } tardis(t, start_time);
  ScopeGuard sg(t);
  t.current_tock++;
  std::vector<std::shared_ptr<EZombieNode>> input_zombie;
  std::vector<const void*> in;
  for (const tock& input : inputs) {
    if (!t.akasha.has_precise(input)) {
      t.akasha.put({input, input + 1}, std::make_unique<GraveYard>());
    }
    auto& n = t.akasha.get_precise_node(input);
    auto ezn = non_null(dynamic_cast<GraveYard*>(n.value.get()))->arise(n);
    in.push_back(ezn->get_ptr());
    input_zombie.push_back(ezn);
  }
  f(in);
}

void zombie_link_test() {
  std::cout << "zombie link ok!" << std::endl;
}
