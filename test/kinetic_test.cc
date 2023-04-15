#include "zombie/kinetic.hpp"

#include <gtest/gtest.h>

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



template<bool hanger, bool is_unique>
void HeapTest() {
  std::vector<int> v = {94, 21, 19, 26, 5, 87, 80, 93, 60, 77, 24, 10, 82, 92, 17, 40, 11, 98, 42, 78};

  using E = Element<is_unique>;
  MinHeap<E, hanger> h;
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
  HeapTest<true , false>();
  HeapTest<false, false>();
}

TEST(HeapTest, UniqueElement) {
  HeapTest<true , true>();
  HeapTest<false, true>();
}





template<bool is_unique>
struct NotifyIndexChanged<Element<is_unique>> {
    void operator()(const Element<is_unique>&, const size_t &) {}
};

template<bool hanger, bool is_unique>
void KineticHeapTest() {
  std::vector<std::pair<int, int>> v0 = {{5, 20}, {1, 7}, {2, 19}, {11, 12}, {10, 16}, {14, 13}, {9, 3}, {4, 15}, {18, 17}, {8, 6}};
  std::vector<std::pair<int, int>> v1 = {{17, 8}, {6, 13}, {10, 12}, {7, 7}, {3, 9}, {20, 14}, {18, 15}, {19, 2}, {1, 16}, {11, 5}};
  int numbering = 0;

  using E = Element<is_unique>;
  KineticMinHeap<E, hanger> h(0);
  FakeKineticMinHeap<E> fake_h(0);
  for (const auto& p: v0) {
    h.push(E { numbering }, AffFunction(p.first, p.second));
    fake_h.push(E { numbering }, AffFunction(p.first, p.second));
    ++numbering;
    EXPECT_EQ(h.size(), fake_h.size());
  }
  size_t time = 5;
  h.advance_to(time);
  fake_h.advance_to(time);
  for (size_t i = 0; i < 5; ++i) {
    EXPECT_EQ(h.pop().get(), fake_h.pop().get());
    EXPECT_EQ(h.size(), fake_h.size());
  }
  for (const auto& p: v1) {
    h.push(E { numbering }, AffFunction(p.first, p.second));
    fake_h.push(E { numbering }, AffFunction(p.first, p.second));
    ++numbering;
    EXPECT_EQ(h.size(), fake_h.size());
  }
  while (!h.empty()) {
    time += 5;
    h.advance_to(time);
    fake_h.advance_to(time);
    EXPECT_EQ(h.pop().get(), fake_h.pop().get());
    EXPECT_EQ(h.size(), fake_h.size());
  }
}

TEST(KineticHeapTest, Kinetic) {
  KineticHeapTest<true , false>();
  KineticHeapTest<false, false>();
}

TEST(KineticHeapTest, UniqueElement) {
  KineticHeapTest<true , true>();
  KineticHeapTest<false, true>();
}
