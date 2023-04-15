#pragma once

#include <vector>
#include <set>
#include <cstddef>

// May god forgive my sin.
template<typename T>
struct NotifyIndexChanged; //{
//  void operator()(const T& t, size_t idx) { }
//};


// an unordered container of T.
// allow constant time insert, remove, and lookup.
template<typename T>
struct Bag {
private:
  std::vector<T> vec;

public:
  Bag() { }
  Bag(std::initializer_list<T> l) : vec(l) { }

  void insert(const T& t) {
    vec.push_back(t);
    notify(vec.back(), vec.size() - 1);
  }
  void insert(T&& t) {
    vec.push_back(std::move(t));
    notify(vec.back(), vec.size() - 1);
  }

  T remove(size_t i) {
    std::swap(vec[i], vec.back());
    notify(vec[i], i);
    T t = std::move(vec.back());
    vec.pop_back();
    return t;
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

private:
  static void notify(const T& t, const size_t& idx) {
    NotifyIndexChanged<T>()(t, idx);
  }
};
