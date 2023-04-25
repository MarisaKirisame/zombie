#pragma once

#include "basic.hpp"
#include "aff_function.hpp"


namespace HeapImpls {

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
    heap.push({t, f(time_), f});
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

}; // end of namespace HeapImpls
