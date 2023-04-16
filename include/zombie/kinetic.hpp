#pragma once

#include "heap.hpp"
#include "common.hpp"

#include <cstdint>
#include <optional>
#include <cassert>
#include <vector>
#include <iostream>
#include <unordered_set>
#include <functional>

template<typename T>
T div_ceiling(T x, T y) {
  if (y < 0) {
    x = -x;
    y = -y;
  }
  if (x > 0) {
    return ((x - 1) / y) + 1;
  } else {
    return x / y;
  }
}

using slope_t = int128_t;
using shift_t = int64_t;
using aff_t = int128_t;
// An affine function.
// Note that it is in i64_t,
// because floating point scare me.
struct AffFunction {
  // Typically we have y_shift instead of x_shift.
  // We choose x_shift as this make updating latency much easier.
  // This should not effect the implementation of kinetc data structure in anyway.
  slope_t slope;
  shift_t x_shift;

  friend std::ostream& operator<<(std::ostream& os, const AffFunction& f);

  AffFunction(slope_t slope, shift_t x_shift) : slope(slope), x_shift(x_shift) { }

  aff_t operator()(shift_t x) const {
    auto ret = slope * (x + x_shift);
    // technically speaking I cant check overflow like this as overflow is UB.
    assert((slope == 0) || (ret / slope == (x + x_shift)));
    return ret;
  }

  std::optional<aff_t> lt_until(const AffFunction& rhs) const {
    auto postcondition_check = [&](aff_t val) {
      assert((*this)(val - 1) < rhs(val - 1));
      assert(!((*this)(val) < rhs(val)));
      return std::optional<aff_t>(val);
    };
    shift_t x_delta = rhs.x_shift - x_shift;
    aff_t y_delta = rhs.slope * x_delta;
    // note that x_delta is based on rhs, but slope_delta is based on *this.
    slope_t slope_delta = slope - rhs.slope;
    if (slope_delta <= 0) {
      return std::optional<aff_t>();
    } else {
      assert(slope_delta > 0);
      return postcondition_check(-x_shift + div_ceiling(y_delta, slope_delta));
    }
  }

  // return Some(x) if,
  //   forall x' < x,
  //     (*this)(x') <= rhs(x') /\
  //     !((*this)(x) <= rhs(x)).
  //   note that due to property of Affine Function,
  //     forall x' > x,
  //     !((*this)(x') <= rhs(x'))
  // return None if such x does not exist.
  std::optional<aff_t> le_until(const AffFunction& rhs) const {
    auto postcondition_check = [&](aff_t val) {
      assert((*this)(val - 1) <= rhs(val - 1));
      assert(!((*this)(val) <= rhs(val)));
      return std::optional<aff_t>(val);
    };
    // let's assume the current time is -x_shift,
    //   so eval(cur_time) == 0, and rhs.eval(cur_time) == rhs.slope * x_delta.
    // we then make lhs catch up(or fall back) to rhs
    shift_t x_delta = rhs.x_shift - x_shift;
    aff_t y_delta = rhs.slope * x_delta;
    // note that x_delta is based on rhs, but slope_delta is based on *this.
    slope_t slope_delta = slope - rhs.slope;
    if (slope_delta <= 0) {
      return std::optional<aff_t>();
    } else {
      assert(slope_delta > 0);
      return postcondition_check(-x_shift + div_ceiling(y_delta + 1, slope_delta));
    }
  }

  std::optional<aff_t> gt_until(const AffFunction& rhs) const {
    return rhs.lt_until(*this);
  }

  std::optional<aff_t> ge_until(const AffFunction& rhs) const {
    return rhs.le_until(*this);
  }

};

inline std::ostream &operator<<(std::ostream &out, int128_t val) {
  assert(val <= std::numeric_limits<int64_t>::max());
  return out << static_cast<uint64_t>(val);
}

inline std::ostream& operator<<(std::ostream& os, const AffFunction& f) {
  return os << "(" << f.slope << "(x+" << f.x_shift << "))";
}

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


template<typename T>
struct NotifyIndexChanged; // {
//  void operator()(const T&, const size_t&);
//};

template<typename T, bool hanger>
struct KineticMinHeap {
public:
  size_t size() const {
    return heap.size();
  }

  bool empty() const {
    return heap.empty();
  }

  void push(const T& t, const AffFunction& f) {
    heap.push(Node{t, f});
    recert();
    invariant();
  }

  void push(T&& t, const AffFunction& f) {
    heap.push(Node{std::forward<T>(t), f});
    recert();
    invariant();
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

  size_t min_idx() {
    return 0;
  }

  AffFunction get_aff(size_t i) {
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
    while (!cert_queue.empty()) {
      Certificate c = cert_queue.peek();
      if (c.break_time <= time_) {
        fix(c.heap_idx);
        cert_queue.pop();
      } else {
        break;
      }
    }
  }

  KineticMinHeap(int64_t time) :
    time_(time),
    heap(CompareNode{*this}, NodeIndexChanged{*this}, NodeElementRemoved{*this}),
    cert_queue(CompareCertificate(), CertificateIndexChanged{*this}, CertificateElementRemoved{*this}) { }

private:
  using self_t = KineticMinHeap<T, hanger>;

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
      NotifyIndexChanged<T>()(n.t, idx);
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
    int64_t break_time;
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

  MinHeap<Node, hanger, CompareNode, NodeIndexChanged, NodeElementRemoved> heap;
  // do not use hanger - hanger is only useful in kinetic setting.
  MinHeap<Certificate, false, CompareCertificate, CertificateIndexChanged, CertificateElementRemoved> cert_queue;


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
    cert_invariant_no_dup();
    cert_invariant_heap_cert_id();
    if (!cert_queue.empty()) {
      assert(cert_queue.peek().break_time > time_);
    }
  }

  void invariant() {
    cert_invariant();
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
        if (cert_idx != -1 && break_time) {
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

  void recert() {
    cert_invariant();
    for (size_t idx: pending_recert) {
      recert(idx);
      cert_invariant();
    }
    pending_recert.clear();
  }


  // note: others priority invariant are also broken when we fix(idx).
  //   it is fine, as we will call fix on them individually.
  //   in general it is not fine though.
  void fix(const size_t& idx) {
    heap.flow(idx, false);
  }
};


template<typename T>
struct FakeKineticMinHeap {
  struct Node {
    T t;
    aff_t value;
    AffFunction f;
  };

  struct CompareNode {
    bool operator()(const Node& l, const Node& r) {
      return l.value < r.value;
    }
  };

  struct NodeIndexChanged {
    void operator()(const Node& n, const size_t& idx) { }
  };

  struct NodeElementRemoved {
    void operator()(const Node& n) { }
  };

  MinHeap<Node, false, CompareNode, NodeIndexChanged, NodeElementRemoved> heap;

  int64_t time_;

  FakeKineticMinHeap(int64_t time) : time_(time) { }

  void push(const T& t, const AffFunction& f) {
    heap.push({t, f(time), f});
  }

  void push(T&& t, const AffFunction& f) {
    heap.push({std::forward<T>(t), f(time_), f});
  }

  size_t size() const {
    return heap.size();
  }

  bool empty() const {
    return heap.empty();
  }

  const T& peek() const {
    return heap.peek().t;
  }

  T pop() {
    return heap.pop().t;
  }

  int64_t time() const {
    return time_;
  }

  void advance_to(int64_t new_time) {
    assert(new_time >= time_);
    time_ = new_time;

    MinHeap<Node, false, CompareNode, NodeIndexChanged, NodeElementRemoved> new_heap;
    while (!heap.empty()) {
      auto n = heap.pop();
      n.value = n.f(time_);
      new_heap.push(std::move(n));
    }

    heap = std::move(new_heap);
  }
};
