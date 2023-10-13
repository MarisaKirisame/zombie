#pragma once

#include <random>
#include <optional>
#include <cassert>

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

  template<typename F>
  void remap(const F& f) {
    remap_recurse(f, 0);
  }

  template<typename F>
  void remap_recurse(const F& f, size_t idx) {
    if (self().has_value(idx)) {
      f(self()[idx]);
      remap_recurse(f, heap_left_child(idx));
      remap_recurse(f, heap_right_child(idx));
      sink(idx, false);
    }
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
    if (idx < arr.size()) {
      this->rebalance(idx, true);
      this->notify_changed(idx);
    }
    this->notify_removed(ret);
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
      arr.resize(idx * 2 + 1);
    }
    if (arr[idx].has_value()) {
      // TODO: huh, is 'prioritizing an empty child' a worthwhile optimization?
      size_t child_idx = heap_left_child(idx) + (coin() ? 0 : 1);
      if (cmp(t, arr[idx].value())) {
        T t0 = std::move(arr[idx].value());
        arr[idx] = std::move(t);
        this->notify_changed(idx);
        hang(std::move(t0), child_idx);
      }
      else {
        hang(std::forward<T>(t), child_idx);
      }
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
