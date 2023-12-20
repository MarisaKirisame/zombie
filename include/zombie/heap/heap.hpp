#pragma once

#include <random>
#include <optional>
#include <cassert>
#include <iostream>

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

template<typename T>
struct NotifyHeapIndexChanged; //{
//  void operator()(const T&, const size_t&);
//};

template<typename T>
struct NotifyHeapElementRemoved; //{
//  void operator()(const T&);
//};

template<typename T,
	 typename Compare = std::less<T>,
	 typename NHIC = NotifyHeapIndexChanged<T>,
	 typename NHER = NotifyHeapElementRemoved<T>>
struct MinHeap {
  // maybe we should use a rootish array?
  std::vector<T> arr;

  T& peek() {
    return (*this)[0];
  }

  const T& peek() const {
    return (*this)[0];
  }

  T pop() {
    return remove(0);
  }

  template<typename F>
  void remap(const F& f) {
    remap_recurse(f, 0);
  }

  template<typename F>
  void remap_recurse(const F& f, size_t idx) {
    if (has_value(idx)) {
      f((*this)[idx]);
      remap_recurse(f, heap_left_child(idx));
      remap_recurse(f, heap_right_child(idx));
      sink(idx, false);
    }
  }

  void flow(const size_t& idx, bool idx_notified) {
    assert(has_value(idx));
    if (!heap_is_root(idx)) {
      size_t pidx = heap_parent(idx);
      if (cmp((*this)[idx], (*this)[pidx])) {
        swap(idx, pidx);
        if (!idx_notified) {
          notify_changed(idx);
        }
        flow(pidx, true);
        notify_changed(pidx);
      }
    }
  }

  void sink(const size_t& idx, bool idx_notified) {
    size_t a_child_idx = heap_left_child(idx);
    size_t b_child_idx = heap_right_child(idx);
    if (has_value(a_child_idx) || has_value(b_child_idx)) {
      size_t smaller_idx = [&](){
        if (!has_value(a_child_idx)) {
          return b_child_idx;
        } else if (!has_value(b_child_idx)) {
          return a_child_idx;
        } else {
          return cmp((*this)[a_child_idx], (*this)[b_child_idx]) ? a_child_idx : b_child_idx;
        }
      }();
      if (cmp((*this)[smaller_idx], (*this)[idx])) {
        swap(idx, smaller_idx);
        if (!idx_notified) {
          notify_changed(idx);
        }
        sink(smaller_idx, true);
        notify_changed(smaller_idx);
      }
    }
  }

  void rebalance(const size_t& idx, bool idx_notified) {
    assert(has_value(idx));
    flow(idx, idx_notified);
    sink(idx, idx_notified);
  }

  void notify_changed(const size_t& i) {
    assert(has_value(i));
    nhic(static_cast<const T&>((*this)[i]), i);
  }

  void notify_removed(const T& t) {
    nher(t);
  }

  void swap(const size_t& l, const size_t& r) {
    std::swap(arr[l], arr[r]);
  }

  bool empty() const {
    return arr.empty();
  }

  size_t size() const {
    return arr.size();
  }

  void clear() {
    arr.clear();
  }

  void push(const T& t) {
    arr.push_back(t);
    flow(arr.size() - 1, true);
    notify_changed(arr.size() - 1);
  }

  void push(T&& t) {
    arr.push_back(std::forward<T>(t));
    flow(arr.size() - 1, true);
    notify_changed(arr.size() - 1);
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

  T remove_no_rebalance(const size_t& idx) {
    assert(has_value(idx));
    T ret = std::move(arr[idx]);
    swap(idx, arr.size() - 1);
    arr.pop_back();
    notify_removed(ret);
    return ret;
  }

  T remove(const size_t& idx) {
    T ret = remove_no_rebalance(idx);
    if (idx < arr.size()) {
      rebalance(idx, true);
      notify_changed(idx);
    }
    return ret;
  }

  void heapify() {
    heapify_recurse(0);
  }

  void heapify_recurse(size_t idx) {
    if (has_value(idx)) {
      heapify_recurse(heap_left_child(idx));
      heapify_recurse(heap_right_child(idx));
      sink(idx, false);
    }
  }
  template<typename F, typename O>
  void remove_if(const F& f, const O& o) {
    size_t idx = 0;
    while (idx < arr.size()) {
      if (f(arr[idx])) {
        o(remove_no_rebalance(idx));
      } else {
        ++idx;
      }
    }
    heapify();
  }

  Compare cmp;
  NHIC nhic;
  NHER nher;

  MinHeap(const Compare& cmp = Compare(),
          const NHIC& nhic = NHIC(),
          const NHER& nher = NHER()) : cmp(cmp), nhic(nhic), nher(nher) { }

  std::vector<T> values() const {
    return arr;
  }
};
