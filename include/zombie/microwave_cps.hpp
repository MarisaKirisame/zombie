namespace ZombieInternal {

namespace CPS {

// A MicroWave record a computation executed by bindZombie, to replay it.
// Note that a MicroWave may invoke more bindZombie, which may create MicroWave.
// When that happend, the outer MicroWave will not replay the inner one,
// And the metadata is measured accordingly.
// Note: MicroWave might be created or destroyed,
// Thus the work executed by the parent MicroWave will also change.
// The metadata must be updated accordingly.
template<const ZombieConfig& cfg>
struct MicroWave {
public:
  // we dont really use the Tock return type, but this allow one less boxing.
  std::function<Trampoline::Output<Tock>(const std::vector<const void*>& in)> f;
  std::vector<Tock> inputs;
  Tock output;
  Tock start_time;
  Tock end_time;

  Space space_taken;
  Time time_taken;

  mutable std::vector<Tock> used_by;

  mutable Time last_accessed;

  mutable bool evicted = false;
  mutable ptrdiff_t pool_index = -1;

  // [_set_parent == start_time] when [*this] is a UF root
  mutable Tock _set_parent;
  // the total cost of the UF class of [*this].
  // meaningful only when [*this] is a UF root.
  mutable Time _set_cost;

public:
  MicroWave(std::function<Trampoline::Output<Tock>(const std::vector<const void*>& in)>&& f,
            const std::vector<Tock>& inputs,
            const Tock& output,
            const Tock& start_time,
            const Tock& end_time,
            const Space& space,
            const Time& time_taken);

  static Trampoline::Output<Tock> play(const std::function<Trampoline::Output<Tock>(const std::vector<const void*>& in)>& f,
                                       const std::vector<Tock>& inputs);
  cost_t cost() const;
  void replay();

  void accessed() const;

  void evict();

  Tock root_of_set() const;
  Time cost_of_set() const;

private:
  std::pair<Tock, Time> info_of_set() const;

  void merge_with(Tock);
};

template<const ZombieConfig& cfg>
struct MicroWavePtr : public std::shared_ptr<MicroWave<cfg>> {
  MicroWavePtr(std::function<Trampoline::Output<Tock>(const std::vector<const void*>& in)>&& f,
               const std::vector<Tock>& inputs,
               const Tock& output,
               const Tock& start_time,
               const Tock& end_time,
               const Space& space,
               const Time& time_taken);

  void replay();
};

} // end of namespace CPS

} // end of namespace ZombieInternal
