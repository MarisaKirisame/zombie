#include <gtest/gtest.h>

#include "zombie/heap/heap.hpp"
#include "common.hpp"


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

template<typename Heap, bool is_unique>
void HeapTest() {
  std::vector<int> v = {94, 21, 19, 26, 5, 87, 80, 93, 60, 77, 24, 10, 82, 92, 17, 40, 11, 98, 42, 78};

  using E = Element<is_unique>;
  Heap h;
  size_t size = 0;
  for (int i : v) {
    h.push(E{i});
    ++size;
    assert(h.size() == size);
  }
  std::sort(v.begin(), v.end());
  std::vector<int> ret;
  while (!h.empty()) {
    ret.push_back(h.pop().get());
    --size;
    assert(h.size() == size);
  }
  EXPECT_EQ(ret.size(), v.size());
  for (size_t i = 0; i < ret.size(); ++i) {
    EXPECT_EQ(ret[i], v[i]);
  }
}

TEST(HeapTest, Kinetic) {
  HeapTest<MinHeap<Element<false>, true >, false>();
  HeapTest<MinHeap<Element<false>, false>, false>();
}

TEST(HeapTest, UniqueElement) {
  HeapTest<MinHeap<Element<true>, true >, true>();
  HeapTest<MinHeap<Element<true>, false>, true>();
}
