#pragma once

#include "aff_function.hpp"

#include <vector>
#include <set>
#include <cstddef>
#include <functional>



namespace HeapImpls {

// an unordered container of T.
// allow constant time insert, remove, and lookup.
template<typename T, typename NotifyIndexChanged>
struct Bag {
private:
  struct Node {
    T t;
    AffFunction f;
  };

  std::vector<Node> vec;
  int64_t time_;

public:
  Bag(int64_t time) : time_(time) {}

  size_t size() const {
    return vec.size();
  }

  bool empty() const {
    return size() == 0;
  }

  void push(const T& t, const AffFunction &f) {
    vec.push_back(Node { t, f });
    notify(vec.back(), vec.size() - 1);
  }
  void push(T&& t, const AffFunction &f) {
    vec.push_back(Node { std::move(t), f });
    notify(vec.back(), vec.size() - 1);
  }


  size_t min_idx() const {
    size_t min_idx = vec.size();
    for (size_t i = 0; i < vec.size(); ++i)
      if (min_idx >= vec.size() || vec[i].f(time_) <= vec[min_idx].f(time_))
        min_idx = i;

    assert(min_idx < vec.size());
    return min_idx;
  }


  T& peek() {
    return vec[min_idx()].t;
  }

  const T& peek() const {
    return vec[min_idx()].t;
  }

  T pop() {
    return remove(min_idx());
  }

  T remove(size_t i) {
    if (i == 0 && vec.size() == 1) {
      T t = std::move(vec.back().t);
      vec.pop_back();
      return t;
    }
    std::swap(vec[i], vec.back());
    notify(vec[i], i);
    T t = std::move(vec.back().t);
    vec.pop_back();
    return t;
  }


  T& operator[](size_t i) {
    return vec[i].t;
  }

  const T& operator[](size_t i) const {
    return vec[i].t;
  }


  AffFunction get_aff(size_t i) const {
    return vec[i].f;
  }

  void set_aff(size_t i, const AffFunction& f) {
    vec[i].f = f;
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
  }



  operator std::set<T>() const {
    return std::set<T>(vec.begin(), vec.end());
  }

  bool operator==(const Bag<T, NotifyIndexChanged>& rhs) const {
    return std::set<T>(*this) == std::set<T>(rhs);
  }

  bool operator!=(const Bag<T, NotifyIndexChanged>& rhs) const {
    return std::set<T>(*this) != std::set<T>(rhs);
  }

private:
  static void notify(const Node& n, const size_t& idx) {
    NotifyIndexChanged()(n.t, idx);
  }
};

}; // end of namespace HeapImpls
