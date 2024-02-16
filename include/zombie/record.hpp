#pragma once

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

} // end of namespace ZombieInternal
