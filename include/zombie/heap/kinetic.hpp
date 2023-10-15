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
  size_t size() const {
    return heap.size() + nursery.size();
  }

  bool empty() const {
    return heap.empty() && nursery.empty();
  }

  void push(const T& t, const AffFunction& f) {
    T t_(t);
    push(std::move(t_), f);
  }

  void push(T&& t, const AffFunction& f) {
    if ((!heap.empty()) && f(time()) >= threshold && f.slope < 0) {
      nursery.push(Young(std::move(t), f, threshold));
    } else {
      heap.push(Node{std::move(t), f});
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
    }
    promote();
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
  // it may be good to have multiple generation.
  // note that we can demote a value from old gen to young gen... super weird.

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

  MinHeap<Young, false, CompareYoung, YoungIndexChanged, YoungElementRemoved> nursery;

  // when heap is empty threshold is invalid.
  aff_t threshold;

  template<typename I>
  static I biggerMag(I num, double factor) {
    return num > 0 ? num * factor : num / factor;
  }

  template<typename I>
  static I smallerMag(I num, double factor) {
    return num > 0 ? num / factor : num * factor;
  }

  constexpr static double threshold_factor = 4;

private:
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
    if (!heap.empty()) {
      aff_t current_value = heap.peek().f(time());
      assert(current_value < threshold);
      for (size_t i = 0; i < nursery.size(); ++i) {
        assert(nursery[i].promote_time > time());
        assert(nursery[i].aff(time()) > current_value);
      }
    }
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
    nursery.clear();
  }

  void promote() {
    while ((!nursery.empty()) && nursery.peek().promote_time <= time()) {
      Young yg = nursery.pop();
      heap.push(Node{std::move(yg.t), yg.aff});
      recert();
    }
  }

  void reset_threshold(aff_t threshold) {
    if (this->threshold != threshold) {
      std::cout << "reset threshold!" << std::endl;
      this->threshold = threshold;
      nursery.remap([&](Young& y) { y.promote_time = y.aff.ge_until(threshold).value(); });
      promote();
    }
  }

  void recert() {
    cert_invariant();
    while (!pending_recert.empty()) {
        tmp.clear();
        tmp.swap(pending_recert);
        for (size_t idx: tmp) {
          recert(idx);
          cert_invariant();
      }
    }
    if (heap.empty()) {
      if (!nursery.empty()) {
        Young y = nursery.pop();
        push(std::move(y.t), y.aff);
      }
    }
    if (!heap.empty()) {
      aff_t current_value = heap.peek().f(time());
      if (heap.size() == 1 || // if it is nearly empty, try to get some value from the nursery
          threshold < current_value || // the invariant is broken, have to reset it
          threshold > biggerMag(current_value, threshold_factor * threshold_factor)) { // threshold too far, causing value to promote too early.
        reset_threshold(biggerMag(current_value, threshold_factor));
      }
    }
  }


  // note: others priority invariant are also broken when we fix(idx).
  //   it is fine, as we will call fix on them individually.
  //   in general it is not fine though.
  void fix(const size_t& idx) {
    heap.flow(idx, false);
  }
};

} // end of namespace HeapImpls
