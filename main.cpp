struct Record {
  tick_t finished_tick;
  Zombie<_> value;
}

// start at 0.
// a tick pass whenever a Zombie is created.
using tick_t = int64_t;

struct Scope {
  tick_t &tick;
  std::vector<Zombie<_>> created;
};

struct Context {
  std::vector<Scope> scopes;
  tick_t day, night;
}
struct World {
  static World& get_world();
  // maybe this should be an kinetic datastructure, we will see
  std::vector<ERemat> evict_pool;
  // when replaying a function, look at recorded to repatch recursive remat, thus existing fragment can be reused
  std::unordered_map<tick_t, Record> recorded;
  Context ctx;
};

// does not recurse
template<typename T>
struct RematNode {
  std::function<void(...)>;
  std::vector<> input;
  std::vector<> output;
  size_t memory;
  int64_t compute_cost;
  time_t enter_time;
  tick_t started_tick;
};

template<typename T>
struct Remat {
  std::shared_ptr<RematNode<T>> node;
};

template<typename T>
struct ZombieNode {
  std::optional<T> t;
  std::optional<Remat<T>> rm;
  // invariant: t or rm must has_value()
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
