#include "zombie.hpp"

int main() {
  static int destructor_count = 0;
  struct Resource {
    //~Resource() {
    //  ++destructor_count;
    //}
  };
  Zombie<Resource> x;
  Zombie<Resource> y = bindZombie([](const Resource& x) { return Zombie(Resource()); }, x);
}
