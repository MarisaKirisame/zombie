#include "zombie.hpp"

void MicroWave::replay() {
  World& w = World::get_world();
  struct Tardis {
    World& w;
    tock old_tock;
    Tardis(World& w, const tock& new_tock) : w(w), old_tock(w.current_tock) {
      w.current_tock = new_tock;
    }
    ~Tardis() {
      w.current_tock = old_tock;
    }
  } t(w, start_time);
  ScopeGuard sg(w);
  w.current_tock++;
  std::vector<std::shared_ptr<EZombieNode>> input_zombie;
  for (const tock& t : input) {
    if (w.record.has_precise(t)) {
      auto& n = w.record.get_precise_node(t);
      input_zombie.push_back(non_null(dynamic_cast<GraveYard*>(n.value.get()))->arise(n));
    } else {
      ASSERT(false);
    }
  }
  std::cout << "replay" << std::endl;
  throw;
}
/*
  void replay(const tock& start_at) {
  std::vector<std::unique_ptr<EGuard>> guards;
  for (EMonster* em : monster) {
  guards.push_back(std::make_unique<EGuard>(em));
  }
  std::vector<const void*> in;
  for (const auto& z : guards) {
  in.push_back(z->get_ptr());
  }
  f(in);
  }
*/
