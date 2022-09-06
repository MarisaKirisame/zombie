#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>
#include <optional>

struct EZombieNode {
  
};

using EZombie = std::shared_ptr<EZombieNode>;
using WEZombie = std::weak_ptr<EZombieNode>;

struct EComputerNode {
  
};

using EComputer = std::shared_ptr<EComputerNode>;
using WEComputer = std::weak_ptr<EComputerNode>;

// start at 0.
// a tock pass whenever a Computer is created.
using tock_t = int64_t;

struct Record {
  tock_t finished_tock;
  WEZombie value;
};

struct Scope {
  tock_t &tick;
  std::vector<WEZombie> created;
};

struct Context {
  std::vector<Scope> scopes;
  tock_t day, night;
};

struct World {
  static World& get_world();
  // maybe this should be an kinetic datastructure, we will see
  std::vector<WEComputer> evict_pool;
  // when replaying a function, look at recorded to repatch recursive remat, thus existing fragment can be reused
  std::unordered_map<tock_t, Record> recorded;
  Context ctx;
};

// does not recurse
template<typename T>
struct ComputerNode : EComputerNode {
  std::function<void()> f;
  std::vector<EZombie> input;
  std::vector<WEZombie> output;
  size_t memory;
  int64_t compute_cost;
  time_t enter_time;
  tock_t started_tock;
};

template<typename T>
struct Computer {
  std::shared_ptr<ComputerNode<T>> node;
};

template<typename T>
struct ZombieNode : EZombieNode {
  std::optional<T> t;
  std::optional<Computer<T>> com;
  // invariant: t or com must has_value()
};

template<typename T>
struct Zombie {
  std::shared_ptr<ZombieNode<T>> node;
};

template<typename T>
struct Gaurd {
  const T& get();
};

int main() {
  return 0;
}
