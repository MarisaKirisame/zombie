#pragma once

#include "basic.hpp"
#include "aff_function.hpp"

#include <cstdint>
#include <optional>
#include <cassert>
#include <vector>
#include <iostream>
#include <unordered_set>
#include <functional>
#include <list>

template<typename T>
struct NotifyKineticHeapIndexChanged; //{
//  void operator()(const T&, const size_t&);
//};

namespace HeapImpls {

// Note: if this become a bottleneck,
//   read the thesis and <Faster Kinetic Heaps and Their Use in Broadcast Scheduling> more carefully.


// There is four kinetic priority queue:
//   KineticHeap, KineticHanger, KineticHeater, and KineticTournament.
// I think KineticHanger is the best, but since KineticHeap is simpler,
//   and they share lots of similar code, so I will implement both.
// Note: Kinetic Tournament suffer from imbalancing.
//   This is a non-issue in the thesis, because they assume the queue size
//     will not be too far off from the initial size.
//   This is false for us.
//   I am not sure if KineticHeater have this problem or not.
// TODO: implement kinetic heater
// TODO: implement kinetic tournament
template<typename T, bool hanger, typename NotifyIndexChanged=NotifyKineticHeapIndexChanged<T>>
struct KineticMinHeap {
public:
  size_t total_recert = 0;

  size_t size() const {
    return heap.size() + train.size();
  }

  bool empty() const {
    return heap.empty() && train.empty();
  }

  void push(const T& t, const AffFunction& f) {
    T t_(t);
    push(std::move(t_), f);
  }

  bool should_add_to_train(const AffFunction& f) const {
    return Train::use_train && (!heap.empty()) && f.slope < 0 && f(time()) >= bigger_mag(heap.peek().f(time()), Train::threshold_factor);
  }

  void push_main_no_recert(T&& t, const AffFunction& f) {
    heap.push(Node{std::move(t), f});
  }

  void push(T&& t, const AffFunction& f) {
    if (should_add_to_train(f)) {
      train.push(*this, std::move(t), f);
    } else {
      push_main_no_recert(std::move(t), f);
      recert();
      invariant();
    }
  }

  void insert(const T& t, const AffFunction &f) {
    push(t, f);
  }

  void insert(T&& t, const AffFunction &f) {
    push(std::forward<T>(t), f);
  }

  T& peek() {
    return heap[0].t;
  }

  const T& peek() const {
    return heap[0].t;
  }

  T pop() {
    T ret = heap.pop().t;
    recert();
    return ret;
  }

  T remove(size_t i) {
    T ret = heap.remove(i).t;
    recert();
    return ret;
  }

  T& operator[](size_t i) {
    return heap[i].t;
  }

  const T& operator[](size_t i) const {
    return heap[i].t;
  }

  size_t min_idx() const {
    return 0;
  }

  AffFunction get_aff(size_t i) const {
    return heap[i].f;
  }

  void set_aff(size_t i, const AffFunction& f) {
    heap[i].f = f;
    heap.rebalance(i, false);
  }

  void update_aff(size_t i, const std::function<AffFunction(const AffFunction&)>& f) {
    set_aff(i, f(get_aff(i)));
  }

  int64_t time() const {
    return time_;
  }

  void advance_to(int64_t new_time) {
    assert(new_time >= time_);
    time_ = new_time;
    while ((!cert_queue.empty()) && cert_queue.peek().break_time <= time()) {
      Certificate c = cert_queue.pop();
      fix(c.heap_idx);
      total_recert += 1;
    }
    // does not need to recert because we are recerting it at the next line.
    train.time_changed_no_recert(*this);
    recert();
    invariant();
  }

  KineticMinHeap(int64_t time) :
    time_(time),
    heap(CompareNode{*this}, NodeIndexChanged{*this}, NodeElementRemoved{*this}),
    cert_queue(CompareCertificate(), CertificateIndexChanged{*this}, CertificateElementRemoved{*this}) { }

private:
  using self_t = KineticMinHeap<T, hanger, NotifyIndexChanged>;

  struct Node {
    T t;
    AffFunction f;
    ptrdiff_t cert_idx = -1;
  };

  struct CompareNode {
    self_t& h;
    bool operator()(const Node& l, const Node& r) {
      return l.f(h.time_) < r.f(h.time_);
    }
  };

  struct NodeIndexChanged {
    self_t& h;

    void operator()(const Node& n, const size_t& idx) {
      if (n.cert_idx != -1) {
        h.cert_queue[n.cert_idx].heap_idx = idx;
      }
      h.will_recert(idx);
      NotifyIndexChanged()(n.t, idx);
    }
  };

  struct NodeElementRemoved {
    self_t& h;

    void operator()(const Node& n) {
      if (n.cert_idx != -1) {
        h.cert_queue[n.cert_idx].heap_idx = -1;
        h.cert_queue.remove(n.cert_idx);
      }
    }
  };


  struct Certificate {
    shift_t break_time;
    // when we remove a node we remove it's certificate.
    // but when we remove a certificate we change the cert_idx in the node.
    // to avoid that looping back causing bad access, we will set this to -1 when so.
    ptrdiff_t heap_idx;
  };

  struct CompareCertificate {
    bool operator()(const Certificate& l, const Certificate& r) {
      return l.break_time < r.break_time;
    }
  };

  struct CertificateIndexChanged {
    self_t& h;
    void operator()(const Certificate& c, const size_t& idx) {
      h.heap[c.heap_idx].cert_idx = idx;
    }
  };

  struct CertificateElementRemoved {
    self_t& h;
    void operator()(const Certificate& c) {
      if (c.heap_idx != -1) {
        h.heap[c.heap_idx].cert_idx = -1;
      }
    }
  };



private:
  int64_t time_;
  std::unordered_set<size_t> pending_recert;
  std::unordered_set<size_t> tmp;
public:
  MinHeap<Node, hanger, CompareNode, NodeIndexChanged, NodeElementRemoved> heap;
  // do not use hanger - hanger is only useful in kinetic setting.
  MinHeap<Certificate, false, CompareCertificate, CertificateIndexChanged, CertificateElementRemoved> cert_queue;
  // this is a critical optimization:
  // note that we only care about the top element of the kinetic heap,
  // the rest of the heap is pointless beside maintaining the invariant.
  // why do we spend lots of cpu maintaining the certificate between them then?
  // we can keep track of 'when is a number becoming the smallest value', then pushing is only a small constant.
  // but everytime the smallest value change there is a huge cost to remaintain the invariant.
  // instead, we keep track of 'when is a number smaller then a threshold'?
  // when that happends, only then we add the element to a normal kinetic heap.
  // the threshold could be set to e.g. 2x the minimum value (note: value might be negative. in fact for the use case of zombie it is negative.)
  // and when the threshold is smaller then the minimum value we have to reset it.
  // note that this is suprsingly similar with generational garbage collection.
  // note that we can demote a value from old gen to young gen... super weird.

  // the train algorithm.
  // we have a list of car, with each one having a promotion threshold threshold_factor x larger then the front one.
  // each car manage a heap specifying when should each element promote.
  // when the front car's threshold is smaller then the current min value,
  // we have to promote all element in the front car to the kinetic heap, and remove it.
  // when the front car's threshold is much larger then the current min value,
  // we will insert a smaller car, and readjust value from the old car to track promotion correctly.
  // when we have too much car, we will remove the last one, promoting all value.
  // when we dont have enough car and an element is far away from the last car, we will insert another car.
  struct Car {
    struct Young {
      T t;
      AffFunction aff;
      shift_t promote_time;
      // todo: handle the case where ge_until return None
      Young(T&& t, const AffFunction& aff, aff_t threshold) : t(std::move(t)), aff(aff), promote_time(aff.ge_until(threshold).value()) { }
    };

    struct CompareYoung {
      bool operator()(const Young& l, const Young& r) {
        return l.promote_time < r.promote_time;
      }
    };

    struct YoungIndexChanged {
      void operator()(const Young& y, const size_t& idx) { }
    };

    struct YoungElementRemoved {
      void operator()(const Young& y) { }

    };

    // promote when said value is reached
    aff_t promotion_threshold;

    Car(aff_t promotion_threshold) : promotion_threshold(promotion_threshold) { }

    MinHeap<Young, false, CompareYoung, YoungIndexChanged, YoungElementRemoved> nursery;

    void invariant(shift_t time) {
#ifdef ZOMBIE_KINETIC_VERIFY_INVARIANT
      for (size_t i = 0; i < nursery.size(); ++i) {
        assert(nursery[i].promote_time > time);
        assert(nursery[i].aff(time) > promotion_threshold);
      }
#endif
    }

    bool empty() const {
      return nursery.empty();
    }

    size_t size() const {
      return nursery.size();
    }

    void push(T&& t, const AffFunction& f) {
      nursery.push(Young(std::move(t), f, promotion_threshold));
    }

    template<typename F>
    void promote(aff_t time, const F& output_to) {
      while ((!nursery.empty()) && nursery.peek().promote_time <= time) {
        Young yg = nursery.pop();
        output_to(std::move(yg.t), yg.aff);
      }
    }

    template<typename F>
    void promote_all(const F& output_to) {
      while (!nursery.empty()) {
        Young yg = nursery.pop();
        output_to(std::move(yg.t), yg.aff);
      }
    }
  };

  struct Train {
    constexpr static bool use_train = true;
    constexpr static double threshold_factor = 2;
    constexpr static size_t enough_car = 3;
    constexpr static size_t max_car = 10;

    std::list<Car> cars;

    static void pop_head_no_recert(self_t& kh) {
      assert(!kh.train.cars.empty());
      kh.train.cars.front().promote_all([&](T&& t, const AffFunction& aff) { kh.push_main_no_recert(std::move(t), aff); });
      kh.train.cars.pop_front();
    }

    static void pop_tail_no_recert(self_t& kh) {
      assert(!kh.train.cars.empty());
      if (kh.train.cars.size() >= 2) {
        auto it = kh.train.cars.rbegin();
        assert(it != kh.train.cars.rend());
        ++it;
        assert(it != kh.train.cars.rend());
        kh.train.cars.back().promote_all([&](T&& t, const AffFunction& aff) { it->push(std::move(t), aff); });
      } else {
        assert(kh.train.cars.size() == 1);
        pop_head_no_recert(kh);
      }
    }

    static void push_head_no_recert(self_t& kh) {
      assert(!kh.train.cars.empty());
      kh.train.cars.push_front(bigger_mag(kh.train.cars.front().promotion_threshold, threshold_factor));
      while (kh.train.cars.size() > max_car) {
        pop_tail_no_recert(kh);
      }
    }

    static void invariant(self_t& kh) {
#ifdef ZOMBIE_KINETIC_VERIFY_INVARIANT
      assert(kh.train.cars.size() <= max_car);
      if (kh.heap.empty()) {
        assert(kh.train.empty());
      } else {
        aff_t no_smaller_then = kh.cur_min_value();
        for (Car& c : kh.train.cars) {
          // note: an element in a car might be smaller then an element in a front car.
          assert(!(c.promotion_threshold < promotion_threshold));
          no_smaller_then = c.promotion_threshold;
          c.invariant(kh.time());
        }
      }
#endif
    }

    size_t size() const {
      size_t size = 0;
      for (const Car& c : cars) {
        size += c.size();
      }
      return size;
    }

    bool empty() const {
      for (const Car& c : cars) {
        if (!c.empty()) {
          return false;
        }
      }
      return true;
    }

    void debug() const { }

    static void push(self_t& kh, T&& t, const AffFunction& aff) {
      auto cur_value = aff(kh.time());
      if (kh.train.cars.empty()) {
        assert(!kh.heap.empty());
        kh.train.cars.push_back(bigger_mag(kh.cur_min_value(), threshold_factor));
      }
      while (kh.train.cars.size() < enough_car) {
        assert(!kh.train.cars.empty());
        if (bigger_mag(kh.train.cars.back().promotion_threshold, threshold_factor) >= cur_value) {
          kh.train.cars.push_back(bigger_mag(kh.train.cars.back().promotion_threshold, threshold_factor));
        } else {
          break;
        }
      }
      for (auto it = kh.train.cars.rbegin(); it != kh.train.cars.rend(); ++it) {
        if (it->promotion_threshold < cur_value) {
          it->push(std::move(t), aff);
          return;
        }
      }
      assert(!kh.train.cars.empty());
      kh.train.cars.front().push(std::move(t), aff);
    }

    static void time_changed_no_recert(self_t& kh) {
      // let's remove/add new cars, which might cause batch promotion, before we do individual promotion.
      min_value_changed_no_recert(kh);
      // we have to start at the last value, promoting them up, as a value might get promoted multiple time.
      for (auto it = kh.train.cars.rbegin(); it != kh.train.cars.rend(); ++it) {
        auto front_it = it;
        ++front_it;
        if (front_it != kh.train.cars.rend()) {
          it->promote(kh.time(), [&](T&& t, const AffFunction& aff) {
            front_it->push(std::move(t), aff);
          });
        } else {
          it->promote(kh.time(), [&](T&& t, const AffFunction& aff) {
            kh.push_main_no_recert(std::move(t), aff);
          });
        }
      }
    }

    static void min_value_changed_no_recert(self_t& kh) {
      if (!kh.train.cars.empty()) {
        Car& c = kh.train.cars.front();
        assert(!kh.heap.empty());
        auto cur_min_value = kh.cur_min_value();
        if (c.promotion_threshold < cur_min_value) {
          pop_head_no_recert(kh);
        } else if (c.promotion_threshold >= bigger_mag(cur_min_value, threshold_factor * threshold_factor)) {
          push_head_no_recert(kh);
        }
      }
    }

    static void min_value_changed(self_t& kh) {
      min_value_changed_no_recert(kh);
      kh.recert();
    }

  } train;

private:
  aff_t cur_min_value() {
    return heap.peek().f(time());
  }

  void will_recert(const size_t& idx) {
    pending_recert.insert(idx);
    pending_recert.insert(heap_left_child(idx));
    pending_recert.insert(heap_right_child(idx));
  }

  void cert_invariant_no_dup() {
    std::unordered_set<int> heap_idx;
    for (const Certificate& cert: cert_queue.values()) {
      assert(heap_idx.count(cert.heap_idx) == 0);
      heap_idx.insert(cert.heap_idx);
    }
  }

  void cert_invariant_heap_cert_id() {
    for (const Certificate& cert: cert_queue.values()) {
      size_t cert_idx = heap[cert.heap_idx].cert_idx;
      assert(cert_idx != -1);
      assert(cert_queue[cert_idx].heap_idx == cert.heap_idx);
    }
  }

  // should be maintained even when heap break due to time advancement.
  void cert_invariant() {
#ifdef ZOMBIE_KINETIC_VERIFY_INVARIANT
    cert_invariant_no_dup();
    cert_invariant_heap_cert_id();
    if (!cert_queue.empty()) {
      assert(cert_queue.peek().break_time > time_);
    }
#endif
  }

  void invariant() {
#ifdef ZOMBIE_KINETIC_VERIFY_INVARIANT
    cert_invariant();
    train.invariant(*this);
#endif
  }

  void recert(const size_t& idx) {
    if (heap.has_value(idx)) {
      const Node& n = heap[idx];
      ptrdiff_t cert_idx = n.cert_idx;
      if (heap_is_root(idx)) {
        if (cert_idx != -1) {
          cert_queue.remove(cert_idx);
        }
      } else {
        size_t pidx = heap_parent(idx);
        assert(heap.has_value(pidx));
        const Node& p = heap[pidx];
        std::optional<int64_t> break_time = n.f.ge_until(p.f);
        if (break_time && break_time.value() <= time_) {
          fix(idx);
        } else if (cert_idx != -1 && break_time) {
          Certificate& cert = cert_queue[cert_idx];
          assert(cert.heap_idx == idx);
          if (cert.break_time != break_time.value()) {
            cert.break_time = break_time.value();
            cert_queue.rebalance(cert_idx, false);
          }
        } else if (cert_idx == -1 && break_time) {
          assert(idx <= std::numeric_limits<ptrdiff_t>::max());
          cert_queue.push({break_time.value(), static_cast<ptrdiff_t>(idx)});
          assert(n.cert_idx != -1);
        } else if (cert_idx != -1 && (!break_time)) {
          assert(cert_queue[cert_idx].heap_idx == idx);
          cert_queue.remove(cert_idx);
          assert(n.cert_idx == -1);
        } else {
          assert(cert_idx == -1);
          assert(!break_time);
        }
      }
    }
  }

  void clear() {
    heap.clear();
    cert_queue.clear();
    train.clear();
  }

  void recert() {
    cert_invariant();
    // why a while here? wont all problem be done with a single fix?
    // consider a kinetic heap with A < B < C.
    // suppose we advance time such that A < B break, and we swap A with B.
    // note that C might be < A!
    // we do not have transitivity, because even though we have B < C, we no longer have A < B.
    // this is a problem only because we did batch fix instead of the classical fix-at-a-time solution, as in that world we have A = B < C.
    // that solution, while skipping this problem, might have multiple cert break at the same precise time-point,
    // then it degrade to this batch-recert. So we only implemented batch-recert for simplicity and (probably) efficicency.
    // this is correct because we reconsidered all elements with parent changed, and for those elements without change,
    // the old certificate still work.
    while (!pending_recert.empty()) {
      tmp.clear();
      tmp.swap(pending_recert);
      for (size_t idx: tmp) {
        recert(idx);
        cert_invariant();
      }
      tmp.clear();
    }
    while (heap.empty() && (!train.empty())) {
      train.pop_head_no_recert(*this);
    }
    train.min_value_changed(*this);
  }

public:
  void debug() {
    std::cout << "heap size: " << heap.size() << " cert size: " << cert_queue.size() << std::endl;
    train.debug();
  }

  // note: others priority invariant are also broken when we fix(idx).
  //   it is fine, as we will call fix on them individually.
  //   in general it is not fine though.
  void fix(const size_t& idx) {
    heap.flow(idx, false);
  }
};

} // end of namespace HeapImpls
