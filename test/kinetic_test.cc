#include "zombie/heap/heap.hpp"
#include "common.hpp"

#include <gtest/gtest.h>

template<KineticHeapImpl impl, bool is_unique>
void KineticHeapTest() {
  std::vector<std::pair<int, int>> v0 = {{5, 20}, {1, 7}, {2, 19}, {11, 12}, {10, 16}, {14, 13}, {9, 3}, {4, 15}, {18, 17}, {8, 6}};
  std::vector<std::pair<int, int>> v1 = {{17, 8}, {6, 13}, {10, 12}, {7, 7}, {3, 9}, {20, 14}, {18, 15}, {19, 2}, {1, 16}, {11, 5}};
  int numbering = 0;

  using E = Element<is_unique>;
  KineticHeap<impl, E, NotifyIndexChanged<is_unique>> h(0);
  HeapImpls::FakeKineticMinHeap<E> fake_h(0);
  for (const auto& p: v0) {
    h.push(E { numbering }, AffFunction(p.first, p.second));
    fake_h.push(E { numbering }, AffFunction(p.first, p.second));
    ++numbering;
    EXPECT_EQ(h.size(), fake_h.size());
    EXPECT_EQ(h.time(), fake_h.time());
  }
  size_t time = 5;
  h.advance_to(time);
  fake_h.advance_to(time);
  for (size_t i = 0; i < 5; ++i) {
    EXPECT_EQ(h.peek().get(), fake_h.peek().get());
    EXPECT_EQ(h.pop().get(), fake_h.pop().get());
    EXPECT_EQ(h.size(), fake_h.size());
    EXPECT_EQ(h.time(), fake_h.time());
  }
  for (const auto& p: v1) {
    h.push(E { numbering }, AffFunction(p.first, p.second));
    fake_h.push(E { numbering }, AffFunction(p.first, p.second));
    ++numbering;
    EXPECT_EQ(h.size(), fake_h.size());
    EXPECT_EQ(h.time(), fake_h.time());
  }
  while (!h.empty()) {
    time += 5;
    h.advance_to(time);
    fake_h.advance_to(time);
    EXPECT_EQ(h.peek().get(), fake_h.peek().get());
    EXPECT_EQ(h.pop().get(), fake_h.pop().get());
    EXPECT_EQ(h.size(), fake_h.size());
    EXPECT_EQ(h.time(), fake_h.time());
  }
}

TEST(KineticHeapTest, Kinetic) {
  KineticHeapTest<KineticHeapImpl::Bag, false>();
  KineticHeapTest<KineticHeapImpl::Heap, false>();
  KineticHeapTest<KineticHeapImpl::Hanger, false>();
}

TEST(KineticHeapTest, UniqueElement) {
  KineticHeapTest<KineticHeapImpl::Bag, true>();
  KineticHeapTest<KineticHeapImpl::Heap, true>();
  KineticHeapTest<KineticHeapImpl::Hanger, true>();
}
