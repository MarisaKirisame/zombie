#pragma once

#include <memory>

#include "base.hpp"
#include "tock/tock.hpp"
#include "meter.hpp"
#include "config.hpp"
#include "zombie_types.hpp"
#include "heap/gd_heap.hpp"

namespace ZombieInternal {

template<typename T>
struct UF {
  std::shared_ptr<UF> parent;
  T t; // only meaningful when parent.get() == nullptr
};

template<const ZombieConfig& cfg>
Tock tick();

template<const ZombieConfig& cfg>
struct RecordNode {
  Tock t;
  std::vector<std::shared_ptr<EZombieNode<cfg>>> ez;

  ~RecordNode() { }
  RecordNode() : t(tick<cfg>()) { }
  explicit RecordNode(Tock t) : t(t) { }

  virtual void finish() = 0;
  virtual std::shared_ptr<RecordNode<cfg>> resume() = 0;
  virtual void record_space(const Space&) = 0;
  virtual bool playable() = 0;
  virtual Trampoline::Output<EZombie<cfg>> play() = 0;
};

template<const ZombieConfig& cfg>
using Record = std::shared_ptr<RecordNode<cfg>>;

template<const ZombieConfig& cfg>
struct RootRecordNode : RecordNode<cfg> {
  explicit RootRecordNode(const Tock& t) : RecordNode<cfg>(t) { }
  RootRecordNode() { }
  void finish() override;
  Record<cfg> resume() override;
  void record_space(const Space&) override { }
  bool playable() override { return false; }
  Trampoline::Output<EZombie<cfg>> play() override { assert(false); }
};

template<const ZombieConfig& cfg>
struct HeadRecordNode : RecordNode<cfg> {
  // we dont really use the Tock return type, but this allow one less boxing.
  std::function<Trampoline::Output<EZombie<cfg>>(const std::vector<const void*>& in)> f;
  std::vector<EZombie<cfg>> inputs;

  Space space_taken;
  Time start_time;

  HeadRecordNode(std::function<Trampoline::Output<EZombie<cfg>>(const std::vector<const void*>& in)>&& f, std::vector<EZombie<cfg>>&& inputs);
  void finish() override;
  Record<cfg> resume() override;
  void record_space(const Space& s) override { space_taken += s; }
  bool playable() override { return true; }
  Trampoline::Output<EZombie<cfg>> play() override;
};

template<const ZombieConfig& cfg>
struct SpineRecordNode : RecordNode<cfg> {
  Tock head_t;
  std::vector<Tock> inputs;

  Space space_taken;
  Time start_time;
  void record_space(const Space& s) override { space_taken += s; }
};

template<const ZombieConfig& cfg>
struct ContextNode : Object {
  std::vector<std::shared_ptr<EZombieNode<cfg>>> ez;

  explicit ContextNode(std::vector<std::shared_ptr<EZombieNode<cfg>>>&& ez) : ez(std::move(ez)) { }
  virtual void accessed() = 0;
  virtual bool evictable() = 0;
  virtual void evict() = 0;
};

template<const ZombieConfig& cfg>
using Context = std::shared_ptr<ContextNode<cfg>>;

template<const ZombieConfig& cfg>
struct RootContextNode : ContextNode<cfg> {
  explicit RootContextNode(std::vector<std::shared_ptr<EZombieNode<cfg>>>&& ez) : ContextNode<cfg>(std::move(ez)) { }
  void accessed() override { }
  bool evictable() { return false; }
  void evict() { assert(false); }
};

template<const ZombieConfig& cfg>
struct HeadContextNode : ContextNode<cfg> {
  mutable Time last_accessed;

  explicit HeadContextNode(std::vector<std::shared_ptr<EZombieNode<cfg>>>&& ez);
  void accessed() override;
  bool evictable() { return true; }
  void evict() { this->ez.clear(); }
};

template<const ZombieConfig& cfg>
struct SpineContextNode : ContextNode<cfg> { };

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
    void operator()(const std::unique_ptr<Phantom>&) { }
  };

  struct Reaper;
public:
  Tock current_tock = 1;
  SplayList<Tock, Context<cfg>> akasha;
  GDHeap<cfg, std::unique_ptr<Phantom>, NotifyIndexChanged, NotifyElementRemoved> book;
  Record<cfg> record = std::make_shared<RootRecordNode<cfg>>(Tock(0));
  Replay<cfg> replay;
  ZombieMeter meter;
  Reaper reaper = Reaper(*this);

public:
  Trailokya() { }
  ~Trailokya() { }

  static Trailokya& get_trailokya() {
    static Trailokya t;
    return t;
  }

  // return the closest MicroWave holding [t]
  std::shared_ptr<MicroWave<cfg>> get_microwave(const Tock& t) {/*
    TockTreeElem elem = akasha.get_node(t).value;
    if (elem.index() == TockTreeElemKind::MicroWave) {
      return std::get<TockTreeElemKind::MicroWave>(elem);
    } else {
      auto parent = akasha.get_parent(t);
      if (! parent || parent->value.index() == TockTreeElemKind::Nothing) {
        return nullptr;
      } else {
        assert (parent->value.index() == TockTreeElemKind::MicroWave);
        return std::get<TockTreeElemKind::MicroWave>(parent->value);
      }
      }*/
    assert(false);
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
