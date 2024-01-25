#pragma once

namespace ZombieInternal {

namespace ANF {

struct MWState {
  enum Inner {
    Complete_, Partial_, TailCall_
  } inner;
  MWState() = delete;
  explicit MWState(Inner i) : inner(i) { }
  static MWState Complete() {
    return MWState(Complete_);
  }
  static MWState Partial() {
    return MWState(Partial_);
  }
  static MWState TailCall() {
    return MWState(TailCall_);
  }
  bool operator==(const MWState& rhs) const {
    return inner == rhs.inner;
  }
};

// A MicroWave record a computation executed by bindZombie, to replay it.
// Note that a MicroWave may invoke more bindZombie, which may create MicroWave.
// When that happend, the outer MicroWave will not replay the inner one,
// And the metadata is measured accordingly.
// Note: MicroWave might be created or destroyed,
// Thus the work executed by the parent MicroWave will also change.
// The metadata must be updated accordingly.
//
// To support tail call, and to support early return when replaying,
// There are 3 MicroWaveState.
//
// 0: Complete - this is the most common MW, and the only one without tail call/early return.
// 1: Trampoline -
//     During Trampoline, we have a half-constructed microwave,
//     that we know everything except end time and return value.
//     We still put it on the tock tree, with end time being the end time of parent,
//     and when the tail call sequence is finally done, we go up the tock tree fixing Trampoline to Complete.
// 2: Partial -
//     On replay, once we found the value and fill it, it is no longer necessary to keep replaying.
//     In such case on every bindZombie we can return a null value to quit asap.
//     We are still left with a microwave without end/return though.
//     Instead of throwing it away, we put it on the tock tree to speedup lookup.
//     It's end time is the largest time we know is safe - mostly it mean the end time of its children.
//     Subsequent replay might 'complete' the partial, extending it's time.
// Note that Trampoline and Partial might transit into each other.
// However, once Complete there are no more transition.
template<const ZombieConfig& cfg>
struct MicroWave {
public:
  // we dont really use the Tock return type, but this allow one less boxing.
  std::function<Trampoline::Output<Tock>(const std::vector<const void*>& in)> f;
  MWState state;
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
            const MWState& state,
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
               const MWState& state,
               const std::vector<Tock>& inputs,
               const Tock& output,
               const Tock& start_time,
               const Tock& end_time,
               const Space& space,
               const Time& time_taken);

  void replay();
};

} // end of namespace ANF

} // end of namespace ZombieInternal
