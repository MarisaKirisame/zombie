#pragma once

#include <cstdint>
#include <optional>
#include <cassert>
#include <vector>
#include <random>
#include <iostream>
#include <unordered_set>

inline int64_t div_ceiling(int64_t x, int64_t y) {
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

// An affine function.
// Note that it is in i64_t,
// because floating point scare me.
struct AffFunction {
  // Typically we have y_shift instead of x_shift.
  // We choose x_shift as this make updating latency much easier.
  // This should not effect the implementation of kinetc data structure in anyway.
  int64_t slope, x_shift;

  friend std::ostream& operator<<(std::ostream& os, const AffFunction& f);

  AffFunction(int64_t slope, int64_t x_shift) : slope(slope), x_shift(x_shift) { }

  int64_t operator()(int64_t x) const {
    return slope * (x + x_shift);
  }

  std::optional<int64_t> lt_until(const AffFunction& rhs) const {
    auto postcondition_check = [&](int64_t val) {
      assert((*this)(val - 1) < rhs(val - 1));
      assert(!((*this)(val) < rhs(val)));
      return std::optional<int64_t>(val);
    };
    int64_t x_delta = rhs.x_shift - x_shift;
    int64_t y_delta = rhs.slope * x_delta;
    // note that x_delta is based on rhs, but slope_delta is based on *this.
    int64_t slope_delta = slope - rhs.slope;
    if (slope_delta <= 0) {
      return std::optional<int64_t>();
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
  std::optional<int64_t> le_until(const AffFunction& rhs) const {
    auto postcondition_check = [&](int64_t val) {
      assert((*this)(val - 1) <= rhs(val - 1));
      assert(!((*this)(val) <= rhs(val)));
      return std::optional<int64_t>(val);
    };
    // let's assume the current time is -x_shift,
    //   so eval(cur_time) == 0, and rhs.eval(cur_time) == rhs.slope * x_delta.
    // we then make lhs catch up(or fall back) to rhs
    int64_t x_delta = rhs.x_shift - x_shift;
    int64_t y_delta = rhs.slope * x_delta;
    // note that x_delta is based on rhs, but slope_delta is based on *this.
    int64_t slope_delta = slope - rhs.slope;
    if (slope_delta <= 0) {
      return std::optional<int64_t>();
    } else {
      assert(slope_delta > 0);
      return postcondition_check(-x_shift + div_ceiling(y_delta + 1, slope_delta));
    }
  }

  std::optional<int64_t> gt_until(const AffFunction& rhs) const {
    return rhs.lt_until(*this);
  }

  std::optional<int64_t> ge_until(const AffFunction& rhs) const {
    return rhs.le_until(*this);
  }

};

inline std::ostream& operator<<(std::ostream& os, const AffFunction& f) {
  return os << "(" << f.slope << "(x+" << f.x_shift << "))";
}

inline bool heap_is_root(size_t i) {
  return i == 0;
}

inline size_t heap_parent(size_t i) {
  // Heap index calculation assume array index starting from 1.
  return (i + 1) / 2 - 1;
}

inline size_t heap_left_child(size_t i) {
  return (i + 1) * 2 - 1;
}

inline size_t heap_right_child(size_t i) {
  return heap_left_child(i) + 1;
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

// TODO: I think the right way to do this
//   is to make the class take extra params,
//   but this will do for now.
template<typename T>
struct NotifyHeapIndexChanged; //{
//  void operator()(const T&, const size_t&);
//};

template<typename T>
struct NotifyHeapElementRemoved; //{
//  void operator()(const T&);
//};

template<typename T, typename SelfClass>
struct MinHeapCRTP {
  SelfClass& self() {
    return static_cast<SelfClass&>(*this);
  }

  const SelfClass& self() const {
    return static_cast<const SelfClass&>(*this);
  }

  T& peek() {
    return self()[0];
  }

  const T& peek() const {
    return self()[0];
  }

  T pop() {
    return self().remove(0);
  }

  void flow(const size_t& idx, bool idx_notified) {
    assert(self().has_value(idx));
    if (!heap_is_root(idx)) {
      size_t pidx = heap_parent(idx);
      if (self().cmp(self()[idx], self()[pidx])) {
        self().swap(idx, pidx);
        if (!idx_notified) {
          self().notify_changed(idx);
        }
        flow(pidx, true);
        self().notify_changed(pidx);
      }
    }
  }

  void sink(const size_t& idx, bool idx_notified) {
    size_t a_child_idx = heap_left_child(idx);
    size_t b_child_idx = heap_right_child(idx);
    if (self().has_value(a_child_idx) || self().has_value(b_child_idx)) {
      size_t smaller_idx = [&](){
        if (!self().has_value(a_child_idx)) {
          return b_child_idx;
        } else if (!self().has_value(b_child_idx)) {
          return a_child_idx;
        } else {
          return self().cmp(self()[a_child_idx], self()[b_child_idx]) ? a_child_idx : b_child_idx;
        }
      }();
      if (self().cmp(self()[smaller_idx], self()[idx])) {
        self().swap(idx, smaller_idx);
        if (!idx_notified) {
          self().notify_changed(idx);
        }
        sink(smaller_idx, true);
        self().notify_changed(smaller_idx);
      }
    }
  }

  void rebalance(const size_t& idx, bool idx_notified) {
    assert(self().has_value(idx));
    flow(idx, idx_notified);
    sink(idx, idx_notified);
  }

  void notify_changed(const size_t& i) {
    assert(self().has_value(i));
    self().nhic(static_cast<const T&>(self()[i]), i);
  }

  void notify_removed(const T& t) {
    self().nher(t);
  }

};

template<typename T,
	 typename Compare = std::less<T>,
	 typename NHIC = NotifyHeapIndexChanged<T>,
	 typename NHER = NotifyHeapElementRemoved<T>>
struct MinNormalHeap : MinHeapCRTP<T, MinNormalHeap<T, Compare, NHIC, NHER>> {
  // maybe we should use a rootish array?
  std::vector<T> arr;

  void swap(const size_t& l, const size_t& r) {
    std::swap(arr[l], arr[r]);
  }

  bool empty() const {
    return arr.empty();
  }

  size_t size() const {
    return arr.size();
  }

  void push(const T& t) {
    arr.push_back(t);
    this->flow(arr.size() - 1, true);
    this->notify_changed(arr.size() - 1);
  }

  void push(T&& t) {
    arr.push_back(std::forward<T>(t));
    this->flow(arr.size() - 1, true);
    this->notify_changed(arr.size() - 1);
  }

  bool has_value(const size_t& idx) const {
    return idx < arr.size();
  }

  const T& operator[](const size_t& idx) const {
    assert(has_value(idx));
    return arr[idx];
  }

  T& operator[](const size_t& idx) {
    assert(has_value(idx));
    return arr[idx];
  }

  T remove(const size_t& idx) {
    assert(has_value(idx));
    T ret = std::move(arr[idx]);
    swap(idx, arr.size() - 1);
    arr.pop_back();
    this->notify_removed(ret);
    if (idx < arr.size()) {
      this->rebalance(idx, true);
      this->notify_changed(idx);
    }
    return ret;
  }

  Compare cmp;
  NHIC nhic;
  NHER nher;

  MinNormalHeap(const Compare& cmp = Compare(),
                const NHIC& nhic = NHIC(),
                const NHER& nher = NHER()) : cmp(cmp), nhic(nhic), nher(nher) { }

  std::vector<T> values() const {
    return arr;
  }
};



template<typename T,
	 typename Compare = std::less<T>,
	 typename NHIC = NotifyHeapIndexChanged<T>,
	 typename NHER = NotifyHeapElementRemoved<T>>
struct MinHanger : MinHeapCRTP<T, MinHanger<T, Compare, NHIC, NHER>> {
  std::random_device rd;

  bool coin() {
    std::uniform_int_distribution<> distrib(0, 1);
    return distrib(rd) == 0;
  }

  std::vector<std::optional<T>> arr;

  void swap(const size_t& l, const size_t& r) {
    std::swap(arr[l], arr[r]);
  }

  size_t size_ = 0;

  size_t size() const {
    return size_;
  }

  bool empty() const {
    return size() == 0;
  }

  void hang(T&& t, const size_t& idx) {
    if (!(idx < arr.size())) {
      arr.resize(idx + 1);
    }
    if (arr[idx].has_value()) {
      // TODO: huh, is 'prioritizing an empty child' a worthwhile optimization?
      size_t child_idx = heap_left_child(idx) + (coin() ? 0 : 1);
      if (cmp(t, arr[idx].value())) {
        T t0 = std::forward<T>(arr[idx].value());
        arr[idx] = std::forward<T>(t);
        this->notify_changed(idx);
        hang(std::forward<T>(t0), child_idx);
      }
      else
          hang(std::forward<T>(t), child_idx);
    } else {
      arr[idx] = std::forward<T>(t);
      this->notify_changed(idx);
    }
  }

  void push(const T& t) {
    T t_ = t;
    hang(std::move(t_), 0);
    ++size_;
  }

  void push(T&& t) {
    hang(std::forward<T>(t), 0);
    ++size_;
  }

  bool has_value(const size_t& idx) const {
    return idx < arr.size() && arr[idx].has_value();
  }

  const T& operator[](const size_t& idx) const {
    assert(has_value(idx));
    return arr[idx].value();
  }

  const T& peek() const {
    return (*this)[0];
  }

  T& operator[](const size_t& idx) {
    assert(has_value(idx));
    return arr[idx].value();
  }

  void remove_noret(const size_t& idx) {
    assert(has_value(idx));
    size_t child_idx_a = heap_left_child(idx);
    size_t child_idx_b = heap_right_child(idx);
    if ((!has_value(child_idx_a)) && (!has_value(child_idx_b))) {
      arr[idx] = std::optional<T>();
    } else {
      size_t child_idx = [&](){
        if (has_value(child_idx_a) && has_value(child_idx_b)) {
          return cmp(arr[child_idx_a].value(), arr[child_idx_b].value()) ? child_idx_a : child_idx_b;
        } else if (has_value(child_idx_a)) {
          // !has_value(child_idx_b);
          return child_idx_a;
        } else {
          // has_value(child_idx_b) && !has_value(child_idx_a);
          return child_idx_b;
        }
      }();
      arr[idx] = std::move(arr[child_idx]);
      this->notify_changed(idx);
      remove_noret(child_idx);
    }
  }

  // TODO: maybe we should rebuild and shrink once small enough.
  T remove(const size_t& idx) {
    assert(has_value(idx));
    T ret = std::move(arr[idx].value());
    remove_noret(idx);
    --size_;
    this->notify_removed(ret);
    return ret;
  }

  Compare cmp;
  NHIC nhic;
  NHER nher;

  MinHanger(const Compare& cmp = Compare(),
            const NHIC& nhic = NHIC(),
            const NHER& nher = NHER()) : cmp(cmp), nhic(nhic), nher(nher) { }

  std::vector<T> values() {
    std::vector<T> ret;
    for (const std::optional<T>& ot : arr) {
      if (ot) {
        ret.push_back(ot.value());
      }
    }
    return ret;
  }
};

template<typename T,
	 bool hanger,
	 typename Compare = std::less<T>,
	 typename NHIC = NotifyHeapIndexChanged<T>,
	 typename NHER = NotifyHeapElementRemoved<T>>

using MinHeap = std::conditional_t<hanger, MinHanger<T, Compare, NHIC, NHER>, MinNormalHeap<T, Compare, NHIC, NHER>>;






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


  void advance_to(int64_t new_time) {
    assert(new_time >= time);
    time = new_time;
    while (!cert_queue.empty()) {
      Certificate c = cert_queue.peek();
      if (c.break_time <= time) {
        fix(c.heap_idx);
        cert_queue.pop();
      } else {
        break;
      }
    }
  }


  KineticMinHeap(int64_t time) :
    time(time),
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
      return l.f(h.time) < r.f(h.time);
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
  int64_t time;
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
      assert(cert_queue.peek().break_time > time);
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
          cert_queue.push({break_time.value(), idx});
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
    int64_t value;
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

  int64_t time;

  FakeKineticMinHeap(int64_t time) : time(time) { }

  void push(const T& t, const AffFunction& f) {
    heap.push({t, f(time), f});
  }

  void push(T&& t, const AffFunction& f) {
    heap.push({std::forward<T>(t), f(time), f});
  }

  size_t size() const {
    return heap.size();
  }

  bool empty() const {
    return heap.empty();
  }

  T pop() {
    return heap.pop().t;
  }

  void advance_to(int64_t new_time) {
    assert(new_time >= time);
    time = new_time;

    MinHeap<Node, false, CompareNode, NodeIndexChanged, NodeElementRemoved> new_heap;
    while (!heap.empty()) {
      auto n = heap.pop();
      n.value = n.f(time);
      new_heap.push(std::move(n));
    }

    heap = std::move(new_heap);
  }
};
