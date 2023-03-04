#include <iostream>
#include "Zombie/zombie.hpp"

Tock MicroWave::play(const std::function<Tock(const std::vector<const void*>& in)>& f,
                     const std::vector<Tock>& inputs) {
  Trailokya& t = Trailokya::get_trailokya();
  std::vector<std::shared_ptr<EZombieNode>> input_zombie;
  std::vector<const void*> in;
  for (const Tock& input : inputs) {
    auto ezn = EZombie(input).shared_ptr();
    in.push_back(ezn->get_ptr());
    input_zombie.push_back(ezn);
  }
  return f(in);
}

void MicroWave::replay() {
  Trailokya& t = Trailokya::get_trailokya();
  struct Cronus {
    Trailokya& t;
    Tock old_tock;
    Cronus(Trailokya& t, const Tock& new_tock) : t(t), old_tock(t.current_tock) {
      t.current_tock = new_tock;
    }
    ~Cronus() {
      t.current_tock = old_tock;
    }
  } cronus(t, start_time);
  t.current_tock++;
  play(f, inputs);
}

void RecomputeLater::notify_bag_index_changed(size_t idx) {
  non_null(weak_ptr.lock())->pool_index = idx;
}

void zombie_link_test() {
  std::cout << "zombie link ok!" << std::endl;
}

