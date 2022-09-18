#include "zombie.hpp"

int main() {
  Zombie<int> x(3);
  // maybe weird that we allow evicting on input, but input can live out of scope so we have to recompute them anyway.
  x.evict();
  ASSERT(x.get_value(), 3);
}
