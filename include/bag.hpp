#pragma once

#include <vector>
#include <set>

// an unordered container of T.
// allow constant time insert, remove, and lookup.
template<typename T>
struct Bag {
  std::vector<T> vec;
  Bag() { }
  Bag(std::initializer_list<T> l) : vec(l) { }
  void insert(const T& t) {
    vec.push_back(t);
  }
  void remove(size_t i) {
    std::swap(vec[i], vec.back());
    vec.pop_back();
  }
  T& operator[](size_t i) {
    return vec[i];
  }
  size_t size() const {
    return vec.size();
  }
  operator std::set<T>() const {
    return std::set<T>(vec.begin(), vec.end());
  }
  bool operator==(const Bag<T>& rhs) const {
    return std::set<T>(*this) == std::set<T>(rhs);
  }
  bool operator!=(const Bag<T>& rhs) const {
    return std::set<T>(*this) != std::set<T>(rhs);
  }
};
