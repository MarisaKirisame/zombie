#define ZOMBIE_KINETIC_VERIFY_INVARIANT

#include "zombie/heap/heap.hpp"
#include <memory>

#define EXPECT_EQ(L, R) assert(L == R);

template<bool is_unique>
struct Element;

template<>
struct Element<false> {
  int i;

  int get() const { return i; }
  bool operator<(const Element<false> &other) const { return get() < other.get(); }
  bool operator==(const Element<false> &other) const { return get() == other.get(); }
};

template<>
struct Element<true> {
  std::unique_ptr<int> ptr;

  template<typename... Args>
  Element(Args&&... args) : ptr(std::make_unique<int>(std::forward<Args>(args)...)) {}

  int get() const { return *ptr; }
  bool operator<(const Element<true> &other) const { return get() < other.get(); }
  bool operator==(const Element<true> &other) const { return get() == other.get(); }
};


template<bool is_unique>
struct NotifyHeapIndexChanged<Element<is_unique>> {
  void operator()(const Element<is_unique>& n, const size_t& idx) { }
};

template<bool is_unique>
struct NotifyHeapElementRemoved<Element<is_unique>> {
  void operator()(const Element<is_unique>& i) { }
};


template<bool is_unique>
struct NotifyIndexChanged {
  void operator()(const Element<is_unique>&, const size_t&) { }
};


template<KineticHeapImpl impl, bool is_unique>
void KineticHeapTest(bool negative) {
  std::vector<std::pair<int, int>> v0 = {{5, 20}, {1, 7}, {2, 19}, {11, 12}, {10, 16}, {14, 13}, {9, 3}, {4, 15}, {18, 17}, {8, 6}};
  std::vector<std::pair<int, int>> v1 = {{17, 8}, {6, 13}, {10, 12}, {7, 7}, {3, 9}, {20, 14}, {18, 15}, {19, 2}, {1, 16}, {11, 5}};
  int numbering = 0;

  using E = Element<is_unique>;
  KineticHeap<impl, E, NotifyIndexChanged<is_unique>> h(0);
  HeapImpls::FakeKineticMinHeap<E> fake_h(0);
  for (const auto& p: v0) {
    int m = negative ? -1 : 1;
    h.push(E { numbering }, AffFunction(m * p.first, p.second));
    fake_h.push(E { numbering }, AffFunction(m * p.first, p.second));
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

struct NotifyKHInt {
  void operator()(const int&, const size_t&) { }
};

AffFunction aff_y_shift(slope_t k, shift_t y_shift) {
  assert (k != 0);
  return AffFunction { k, (shift_t)(y_shift / k) };
}

int main() {
  {
    KineticHeapTest<KineticHeapImpl::Hanger, false>(true);
  }
}
