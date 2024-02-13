#pragma once

#include <memory>
#include <unordered_set>

#include "base.hpp"
#include "tock/tock.hpp"
#include "meter.hpp"
#include "config.hpp"
#include "zombie_types.hpp"
#include "heap/gd_heap.hpp"
#include "uf.hpp"

namespace ZombieInternal {

template<const ZombieConfig& cfg>
Tock tick();

template<const ZombieConfig& cfg>
using replay_func = std::function<void(const std::vector<const void*>& in)>;

template<const ZombieConfig& cfg>
struct ReplayerNode {
  replay_func<cfg> f;
  std::vector<EZombie<cfg>> in;
  ReplayerNode(replay_func<cfg>&& f, std::vector<EZombie<cfg>> in) : f(std::move(f)), in(std::move(in)) { }
};

template<const ZombieConfig& cfg>
using Replayer = std::shared_ptr<ReplayerNode<cfg>>;

template<const ZombieConfig& cfg>
struct RecordNode {
  Tock t;
  std::vector<std::shared_ptr<EZombieNode<cfg>>> ez;
  size_t space_taken = 0;
  std::unordered_set<Tock> dependencies;
  void register_unrolled(const Tock& tock) {
    if (tock < t) {
      dependencies.insert(tock);
    }
  }

  ~RecordNode() { }
  RecordNode() : t(tick<cfg>()) { }
  explicit RecordNode(Tock t) : t(t) { }

  // a record's function can only be called via records.back()->xxx(...).
  void suspend(const Replayer<cfg>& rec);
  virtual void suspended(const Replayer<cfg>& rep) = 0;
  virtual void resumed() = 0;
  virtual void completed(const Replayer<cfg>& rep) = 0;
  virtual bool is_tailcall() { return false; }

  // can only be called once, deleting this in the process
  void complete(const Replayer<cfg>& rep);
  virtual void replay_finished() {
    assert(false);
    // live inside valuerecordnode
    // Trailokya<cfg>& t = Trailokya<cfg>::get_trailokya();
    // this->completed();
    // this line delete this;
    // t.records.pop_back();
  }
  void finish(const ExternalEZombie<cfg>& z);
  virtual void tailcall(const Replayer<cfg>& rep) { assert(false); }
  virtual void play() { assert(false); }

  virtual bool is_value() { return false; }
  ExternalEZombie<cfg> pop_value();
  virtual ExternalEZombie<cfg> get_value() { assert(false); }
};

template<const ZombieConfig& cfg>
using Record = std::shared_ptr<RecordNode<cfg>>;

template<const ZombieConfig& cfg>
struct RootRecordNode : RecordNode<cfg> {
  explicit RootRecordNode(const Tock& t) : RecordNode<cfg>(t) { }
  RootRecordNode() { }
  void suspended(const Replayer<cfg>& rep) override;
  void completed(const Replayer<cfg>& rep) override { assert(false); }
  void resumed() override;
};

template<const ZombieConfig& cfg>
struct ValueRecordNode : RecordNode<cfg> {
  ExternalEZombie<cfg> eez;

  ValueRecordNode(ExternalEZombie<cfg>&& eez) : eez(std::move(eez)) { }

  void suspended(const Replayer<cfg>& rep) override { assert(false); }
  void completed(const Replayer<cfg>& rep) override { }
  void resumed() override { assert(false); }
  bool is_value() override { return true; }
  ExternalEZombie<cfg> get_value() override { return eez; }
};

template<const ZombieConfig& cfg>
struct HeadRecordNode : RecordNode<cfg> {
  Replayer<cfg> rep;
  bool played = false;

  Time start_time;

  HeadRecordNode(const Replayer<cfg>& rep);

  void suspended(const Replayer<cfg>& rep) override { assert(false); }
  void completed(const Replayer<cfg>& rep) override;
  void resumed() override { assert(false); }
  bool is_tailcall() override { return true; }
  void tailcall(const Replayer<cfg>& rep) override;
  void play() override;
};

template<const ZombieConfig& cfg>
struct ContextNode : Object {
  Tock start_t, end_t; // open-close
  std::vector<std::shared_ptr<EZombieNode<cfg>>> ez;
  size_t ez_space_taken;
  Replayer<cfg> end_rep;

  // this live up here, but evicted_data_dependent live down there,
  // because compute dependencies is about the next checkpoint,
  // while data dependencies is about this checkpoint.
  UF<Time> evicted_compute_dependents = UF<Time>(Time(0));

  explicit ContextNode(const Tock& start_t, const Tock& end_t,
                       std::vector<std::shared_ptr<EZombieNode<cfg>>>&& ez,
                       const size_t& sp,
                       const Replayer<cfg>& rep);

  virtual void accessed() = 0;
  virtual bool evictable() = 0;
  virtual void evict() = 0;
  virtual void evict_individual(const Tock& t) = 0;
  void replay();
  virtual bool is_tailcall() { return false; }
};

template<const ZombieConfig& cfg>
using Context = std::shared_ptr<ContextNode<cfg>>;

template<const ZombieConfig& cfg>
struct RootContextNode : ContextNode<cfg> {
  explicit RootContextNode(const Tock& start_t, const Tock& end_t,
                           std::vector<std::shared_ptr<EZombieNode<cfg>>>&& ez,
                           const size_t& sp,
                           const Replayer<cfg>& rep) : ContextNode<cfg>(start_t, end_t, std::move(ez), sp, rep) { }
  void accessed() override { }
  bool evictable() override { return false; }
  void evict() override { assert(false); }
  void evict_individual(const Tock& t) override { assert(false); }
};

template<const ZombieConfig& cfg>
struct FullContextNode : ContextNode<cfg> {
  std::vector<Tock> dependencies;
  Time time_taken;
  mutable Time last_accessed;

  mutable ptrdiff_t pool_index = -1;

  UFSet<Time> evicted_data_dependents;
  explicit FullContextNode(const Tock& start_t,
                           const Tock& end_t,
                           std::vector<std::shared_ptr<EZombieNode<cfg>>>&& ez,
                           const size_t& sp,
                           const Time& time_taken,
                           const Replayer<cfg>& rep,
                           std::vector<Tock>&& deps);
  ~FullContextNode();

  void accessed() override;
  bool evictable() override { return true; }
  void evict() override;
  void evict_individual(const Tock& t) override;
  Time time_cost();
  Space space_taken();
  cost_t cost();
  bool is_tailcall() override { return true; }
};

// RecomputeLater holds a weak pointer to a MicroWave,
// and is stored in Trailokya::book for eviction.
template<const ZombieConfig& cfg>
struct RecomputeLater : Phantom {
  std::weak_ptr<FullContextNode<cfg>> weak_ptr;

  RecomputeLater(const std::shared_ptr<FullContextNode<cfg>>& ptr) : weak_ptr(ptr) { }
  cost_t cost() const override;
  void evict() override;
  void notify_index_changed(size_t idx) override {
    if (auto ptr = weak_ptr.lock()) {
      ptr->pool_index = idx;
    }
  }
};

template<const ZombieConfig& cfg>
struct Replay {
  Tock forward_at = std::numeric_limits<Tock>::max();
  std::shared_ptr<EZombieNode<cfg>>* forward_to = nullptr;
};

template<const ZombieConfig& cfg>
struct Trailokya {
public:
  struct NotifyIndexChanged {
    void operator()(const std::unique_ptr<Phantom>& p, size_t idx) {
      p->notify_index_changed(idx);
    };
  };

  struct NotifyElementRemoved {
    void operator()(const std::unique_ptr<Phantom>& p) { }
  };

  struct Reaper;
public:
  Tock current_tock = 1;
  SplayList<Tock, Context<cfg>> akasha;
  GDHeap<cfg, std::unique_ptr<Phantom>, NotifyIndexChanged, NotifyElementRemoved> book;
  std::vector<Record<cfg>> records = {std::make_shared<RootRecordNode<cfg>>(Tock(0))};
  std::vector<Replay<cfg>> replays = {Replay<cfg>{}};
  ZombieMeter meter;
  Reaper reaper = Reaper(*this);
  std::function<void()> each_step = [](){};
  Time recompute_time = Time(0);

public:
  Trailokya() { }
  ~Trailokya() { }

  static Trailokya& get_trailokya() {
    static Trailokya t;
    return t;
  }

public:
  struct Reaper {
    Trailokya& t;

    Reaper(Trailokya& t) : t(t) { }

    bool have_soul() {
      return t.book.empty();
    }

    void murder() {
      assert (t.book.size() > 0);
      t.book.adjust_pop([](const std::unique_ptr<Phantom>& p) { return p->cost(); })->evict();
    }

    uint64_t score() {
      return t.book.score();
    }
  };
};

} // end of namespace ZombieInternal
