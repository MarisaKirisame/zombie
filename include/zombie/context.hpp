#pragma once

#include "record.hpp"
#include "base.hpp"
#include "uf.hpp"

namespace ZombieInternal {

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

} // end of namespace ZombieInternal

