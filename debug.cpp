#include "zombie/zombie.hpp"
#include "zombie/kinetic.hpp"

template<>
struct NotifyHeapIndexChanged<int> {
  void operator()(const int& n, const size_t& idx) { }
};

template<>
struct NotifyHeapElementRemoved<int> {
  void operator()(const int& i) { }
};

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

int main() {
  KineticHeapTest<false>();
  KineticHeapTest<true>();
}
