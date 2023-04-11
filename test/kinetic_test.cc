#include "zombie/kinetic.hpp"

#include <gtest/gtest.h>

template<>
struct NotifyHeapIndexChanged<int> {
  void operator()(const int& n, const size_t& idx) { }
};

template<>
struct NotifyHeapElementRemoved<int> {
  void operator()(const int& i) { }
};

TEST(AffTest, Kinetic) {
  std::vector<AffFunction> v {{5,5}, {6,6}, {7,6}};
  for (size_t i = 0; i < v.size(); ++i) {
    for (size_t j = 0; j < i; ++j) {
      EXPECT_TRUE(v[i].lt_until(v[j]).has_value());
      EXPECT_TRUE(v[i].le_until(v[j]).has_value());
    }
  }
  AffFunction f{29, 38};
  AffFunction g{1, 57};
  EXPECT_TRUE(f.lt_until(g).has_value());
  EXPECT_TRUE(f.le_until(g).has_value());
}

template<bool hanger>
void HeapTest() {
  std::vector<int> v = {94, 21, 19, 26, 5, 87, 80, 93, 60, 77, 24, 10, 82, 92, 17, 40, 11, 98, 42, 78};
  MinHeap<int, hanger> h;
  size_t size = 0;
  for (int i : v) {
    h.push(i);
    ++size;
    assert(h.size() == size);
  }
  std::sort(v.begin(), v.end());
  std::vector<int> ret;
  while (!h.empty()) {
    ret.push_back(h.pop());
    --size;
    assert(h.size() == size);
  }
  EXPECT_EQ(ret.size(), v.size());
  for (size_t i = 0; i < ret.size(); ++i) {
    EXPECT_EQ(ret[i], v[i]);
  }
}

TEST(HeapTest, Kinetic) {
  HeapTest<true>();
  HeapTest<false>();
}

template<bool hanger>
void KineticHeapTest() {
  std::vector<std::pair<int, int>> v0 = {{5, 20}, {1, 7}, {2, 19}, {11, 12}, {10, 16}, {14, 13}, {9, 3}, {4, 15}, {18, 17}, {8, 6}};
  std::vector<std::pair<int, int>> v1 = {{17, 8}, {6, 13}, {10, 12}, {7, 7}, {3, 9}, {20, 14}, {18, 15}, {19, 2}, {1, 16}, {11, 5}};
  size_t numbering = 0;
  KineticMinHeap<int, hanger> h(0);
  FakeKineticMinHeap<int> fake_h(0);
  for (const auto& p: v0) {
    h.push(numbering, AffFunction(p.first, p.second));
    fake_h.push(numbering, AffFunction(p.first, p.second));
    ++numbering;
    assert(h.size() == fake_h.size());
  }
  size_t time = 5;
  h.advance_to(time);
  fake_h.advance_to(time);
  for (size_t i = 0; i < 5; ++i) {
    assert(h.pop() == fake_h.pop());
    assert(h.size() == fake_h.size());
  }
  for (const auto& p: v1) {
    h.push(numbering, AffFunction(p.first, p.second));
    fake_h.push(numbering, AffFunction(p.first, p.second));
    ++numbering;
    assert(h.size() == fake_h.size());
  }
  while (!h.empty()) {
    time += 5;
    h.advance_to(time);
    fake_h.advance_to(time);
    assert(h.pop() == fake_h.pop());
    assert(h.size() == fake_h.size());
  }
}

TEST(KineticHeapTest, Kinetic) {
  KineticHeapTest<true>();
  KineticHeapTest<false>();
}
