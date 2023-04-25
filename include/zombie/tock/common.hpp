#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <optional>

// start at 0.
// a tock pass whenever a Computer start execution, or a Zombie is created.
// we denote the start and end(open-close) of Computer execution as a tock_range.
// likewise, Zombie also has a tock_range with exactly 1 tock in it.
// One property of all the tock_range is, there is no overlapping.
// A tock_range might either be included by another tock_range,
// include that tock_range, or have no intersection.
// This allow a bunch of tock_range in a tree.
// From a tock, we can regain a Computer (to rerun function),
// or a Zombie, to reuse values.
// We use tock instead of e.g. pointer for indexing,
// because doing so allow a far more compact representation,
// and we use a single index instead of separate index, because Computer and Zombie relate -
// the outer tock_range node can does everything an inner tock_range node does,
// so one can replay that instead if the more fine grained option is absent.
struct Tock {
  int64_t tock;
  Tock() { }
  Tock(int64_t tock) : tock(tock) { }
  Tock& operator++() {
    ++tock;
    return *this;
  }
  Tock operator++(int) {
    return tock++;
  }
  Tock operator+(Tock rhs) const { return tock + rhs.tock; }
  Tock operator-(Tock rhs) const { return tock - rhs.tock; }
  bool operator!=(Tock rhs) const { return tock != rhs.tock; }
  bool operator==(Tock rhs) const { return tock == rhs.tock; }
  bool operator<(Tock rhs) const { return tock < rhs.tock; }
  bool operator<=(Tock rhs) const { return tock <= rhs.tock; }
  bool operator>(Tock rhs) const { return tock > rhs.tock; }
  bool operator>=(Tock rhs) const { return tock >= rhs.tock; }
};

template<>
struct std::numeric_limits<Tock> {
  static Tock min() {
    return std::numeric_limits<decltype(std::declval<Tock>().tock)>::min();
  }
  static Tock max() {
    return std::numeric_limits<decltype(std::declval<Tock>().tock)>::max();
  }
};



// open-close.
struct TockRange {
  Tock beg, end;
};


template<typename V>
struct TockTreeData {
  TockRange range;
  V value;
};



template<typename K, typename V>
auto largest_value_le(const std::map<K, V>& m, const K& k) {
  auto it = m.upper_bound(k);
  if (it == m.begin()) {
    return m.end();
  } else {
    return --it;
  }
}

template<typename K, typename V>
auto largest_value_le(std::map<K, V>& m, const K& k) {
  auto it = m.upper_bound(k);
  if (it == m.begin()) {
    return m.end();
  } else {
    return --it;
  }
}

inline bool range_dominate(const TockRange& l, const TockRange& r) {
  return l.beg <= r.beg && l.end >= r.end;
}

// for now we dont allow empty range, but the code should hopefully work with them.
inline bool range_ok(const TockRange& r) {
  return r.beg < r.end;
}

inline bool range_nointersect(const TockRange& l, const TockRange& r) {
  return l.end <= r.beg;
}
