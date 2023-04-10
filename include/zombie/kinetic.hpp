#pragma once

#include <cstdint>
#include <optional>
#include <cassert>

// An affine function.
// Note that it is in i64_t,
// because floating point scare me.
struct AffFunction {
  // Typically we have y_shift instead of x_shift.
  // We choose x_shift as this make updating latency much easier.
  // This should not effect the implementation of kinetc data structure in anyway.
  int64_t slope, x_shift;

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
    if (slope_delta == 0) {
      return std::optional<int64_t>();
    } else if (slope_delta > 0) {
      // note that integer division round toward 0, but we want it to round down.
      return postcondition_check(-x_shift + (y_delta - 1) / slope_delta);
    } else {
      // slope_delta < 0
      return postcondition_check(-x_shift -((y_delta - 1) / -slope_delta));
    }
  }
 
  // return Some(x) if,
  //   forall x' < x,
  //     (*this)(x') <= rhs(x') /\
  //     !((*this)(x) <= rhs(x))
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
    if (slope_delta == 0) {
      return std::optional<int64_t>();
    } else if (slope_delta > 0) {
      // note that integer division round toward 0, but we want it to round down.
      return postcondition_check(-x_shift + y_delta / slope_delta);
    } else {
      // slope_delta < 0
      return postcondition_check(-x_shift -(y_delta / -slope_delta));
    }
  }

  std::optional<int64_t> gt_until(const AffFunction& rhs) const {
    return rhs.lt_until(*this);
  }

  std::optional<int64_t> ge_until(const AffFunction& rhs) const {
    return rhs.le_until(*this);
  }

};
/*
// Note: if this become a bottleneck,
//   read the thesis and <Faster Kinetic Heaps and Their Use in Broadcast Scheduling> more carefully.

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

// There is four kinetic priority queue:
//   KineticHeap, KineticHanger, KineticHeater, and KineticTournament.
// I think KineticHanger is the best, but since KineticHeap is simpler,
//   and they share lots of similar code, so I will implement both.
// Note: Kinetic Tournament suffer from imbalancing.
//   This is a non-issue in the thesis, because they assume the queue size
//     will not be too far off from the initial size.
//   This is false for us.
//   I am not sure if KineticHeater have this problem or not.

// note: do not store this into e.g. red black tree or hash, as we only use break_time.
// the correct way is probably to implement comparison operator outside
struct Certificate {
  size_t heap_idx;
};


template<typename T>
struct NotifyHeapIndexChanged; //{
//  void operator()(const std::vector<std::pair<const typename MinHeap<T>::NodeT*, size_t>>&) { }
//};

template<typename T>
struct NotifyHeapElementRemoved; //{
//  void operator()(const typename MinHeap<T>::NodeT&) { }
//};


// todo: we have 3 heaps and potentially more,
// perhaps we should have some code reuse.
template<typename T>
struct MinHeap {
  struct Node {
    T t;
    int64_t key;
  };
  std::vector<Node> arr;

  using notice = std::vector<std::pair<Node*, size_t>>;

  void notify(const notice& n) {
    NotifyHeapIndexChanged<T>()(n);
  }

  void swap(const size_t& l, const size_t& r) {
    std::swap(arr[l], arr[r]);
  }

  bool empty() {
    return arr.empty();
  }

  size_t size() {
    return arr.size();
  }

  bool idx_lt(const size_t& l, const size_t& r) {
    return arr[l] < arr[r];
  }

  void flow(const size_t& idx, notice& n, bool idx_added=false) {
    if (!heap_is_root(idx)) {
      size_t pidx = heap_parent(idx);
      if (idx_lt(idx, pidx)) {
	swap(idx, pidx);
	if (!idx_added) {
	  n.push_back({&arr[idx], idx});
	}
	n.push_back({&arr[pidx], pidx});
	flow(pidx, n, true);
      }
    }
  }

  void sink(const size_t& idx, notice& n, bool idx_added=false) {
    size_t l_child_idx = heap_left_child(idx);
    size_t r_child_idx = heap_right_child(idx);
    if (l_child_idx < arr.size()) {
      size_t smaller_idx = (r_child_idx < arr.size() && idx_lt(r_child_idx, l_child_idx)) ? r_child_idx : l_child_idx;
      if (idx_lt(smaller_idx, idx)) {
	swap(idx, smaller_idx);
	if (!idx_added) {
	  n.push_back({&arr[idx], idx});
	}
	n.push_back({&arr[smaller_idx], smaller_idx});
	sink(smaller_idx, n, true);
      }
    }
  }

  void push(const T& t, const int64_t& key) {
    arr.push_back({t, key});
    notice n;
    flow(arr.size() - 1, n);
    notify(n);
  }

  T peek() {
    return arr[0].t;
  }

  int64_t peek_key() {
    return arr[0].key;
  }

  T pop() {
    return remove(0);
  }

  T remove(const size_t& idx) {
    T ret = std::move(arr[idx].t);
    swap(idx, arr.size() - 1);
    arr.pop_back();
    notice n;
    n.push_back({&arr[idx], idx});
    flow(idx, n, true);
    sink(idx, n, true);
    // todo: one single function, so no broken invariant
    notify(n);
    NotifyHeapElementRemoved(T);
    return ret;
  }
};

template<typename T>
struct KineticMinHeap {
  struct Node {
    T t;
    AffFunction f;
    ptrdiff_t certificate_idx = -1;
  };
  // maybe we should use a rootish array to reduce latency?
  std::vector<Node> arr;

  MinHeap<Certificate> certificate_queue;

  int64_t time;

  using notice = std::vector<std::pair<Node*, size_t>>;

  void update_certificate(size_t idx) {
    assert(!heap_is_root(idx));
    size_t pidx = heap_parent(idx);
    if (certificate_idx != -1) {
      certificate_queue.remove(certificate_idx);
    }

    auto break_time = arr[idx].f.ge_until(arr[pidx].f);
    if (break_time) {
      certificate_queue.push({idx}, break_time.value());
    }
  }

  void notify(const notice& n) {
    for (const auto& p : n) {
      size_t idx = p.second;
      if (!heap_is_root(idx)) {
	update_certificate(idx);
      }
      size_t lcidx = heap_left_child(idx);
      if (lcidx < arr.size()) {
	update_certificate(lcidx);
	size_t rcidx = heap_right_child(idx);
	if (rcidx < arr.size()) {
	  update_certificate(rcidx);
	}
      }
    }
  }

  void swap(const size_t& l, const size_t& r) {
    std::swap(arr[l], arr[r]);
  }

  bool empty() {
    return arr.empty();
  }

  size_t size() {
    return arr.size();
  }

  bool idx_lt(const size_t& l, const size_t& r) {
    return arr[l] < arr[r];
  }

  void flow(const size_t& idx, notice& n, bool idx_added=false) {
    if (!heap_is_root(idx)) {
      size_t pidx = heap_parent(idx);
      if (idx_lt(idx, pidx)) {
	swap(idx, pidx);
	if (!idx_added) {
	  n.push_back({&arr[idx], idx});
	}
	n.push_back({&arr[pidx], pidx});
	flow(pidx, n, true);
      }
    }
  }

  void sink(const size_t& idx, notice& n, bool idx_added=false) {
    size_t l_child_idx = heap_left_child(idx);
    size_t r_child_idx = heap_right_child(idx);
    if (l_child_idx < arr.size()) {
      size_t smaller_idx = (r_child_idx < arr.size() && idx_lt(r_child_idx, l_child_idx)) ? r_child_idx : l_child_idx;
      if (idx_lt(smaller_idx, idx)) {
	swap(idx, smaller_idx);
	if (!idx_added) {
	  n.push_back({&arr[idx], idx});
	}
	n.push_back({&arr[smaller_idx], smaller_idx});
	sink(smaller_idx, n, true);
      }
    }
  }

  void push(const T& t, const int64_t& key) {
    arr.push_back({t, key});
    notice n;
    flow(arr.size() - 1, n);
    notify(n);
  }

  T peek() {
    return arr[0].t;
  }

  T pop() {
    return remove(0);
  }

  T remove(const size_t& idx) {
    T ret = std::move(arr[idx].t);
    swap(idx, arr.size() - 1);
    arr.pop_back();
    notice n;
    n.push_back({&arr[idx], idx});
    flow(idx, n, true);
    sink(idx, n, true);
    notify(n)
    return ret;
  }

  // note: others priority invariant are also broken when we fix(idx).
  //   it is fine, as we will call fix on them individually.
  //   in general it is not fine though.
  void fix(const size_t& idx, notice& n) {
    flow(idx, n);
  }

  void advance_to(int64_t new_time) {
    assert(new_time >= time);
    time = new_time;
    while (!certificate_queue.empty()) {
      Certificate c = certificate_queue.peek();
      if (c.break_time <= time) {
	fix(c.idx);
	certificate_queue.pop();
      } else {
        break;
      }
    }
  }

};

template<typename T>
struct KineticMinHanger {
};
*/
