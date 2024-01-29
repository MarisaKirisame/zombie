#include <gtest/gtest.h>

#include "common.hpp"
#include "zombie/heap/heap.hpp"

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

TEST(HeapTest, Normal) {
  HeapTest<MinHeap<Element<false>>, false>();
}

TEST(HeapTest, UniqueElement) {
  HeapTest<MinHeap<Element<true>>, true>();
}
