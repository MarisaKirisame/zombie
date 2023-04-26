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




AffFunction aff_y_shift(slope_t k, shift_t y_shift) {
  assert (k != 0);
  return AffFunction { k, (shift_t)(y_shift / k) };
}

// test that reproduce a bug of kinetic min heap in commit 8bef729f4b3e8746ccc6edc83dc1e7030937466b
template<KineticHeapImpl impl>
void NotifyBugTest() {
  using E = Element<false>;
  KineticHeap<impl, E, NotifyIndexChanged<false>> h(0);

  // root. always the smallest. no certificates
  h.push(E {0}, aff_y_shift(2, 0));
  // left/right child of root (assume using KineticMinHeap).
  // always smaller than root, so no certificate
  h.push(E {1}, aff_y_shift(2, 10));
  h.push(E {2}, aff_y_shift(2, 10));
  // initially left child of 1.
  // has certificate, will be removed later
  h.push(E {3}, aff_y_shift(-4, 16));
  // initially right child of 1, also the last element of the heap.
  // has certificate, and the certificate should be the last (has latest break time)
  h.push(E {4}, aff_y_shift(-1, 16));

  // initial heap:
  //        0
  //      /   \
  //     1     2
  //    / \
  //   3   4
  if (impl == KineticHeapImpl::Heap) {
    EXPECT_EQ(h[0].get(), 0);
    EXPECT_EQ(h[heap_left_child(0)].get(), 1);
    EXPECT_EQ(h[heap_right_child(0)].get(), 2);
    EXPECT_EQ(h[heap_left_child(heap_left_child(0))].get(), 3);
    EXPECT_EQ(h[heap_right_child(heap_left_child(0))].get(), 4);
  }
  // initial certificate queue:
  //   c3
  //  /
  // c4
 
  // h.remove(3);
}

TEST(KineticHeapTest, NotifyBug) {
  NotifyBugTest<KineticHeapImpl::Bag   >();
  NotifyBugTest<KineticHeapImpl::Heap  >();
  NotifyBugTest<KineticHeapImpl::Hanger>();
}
