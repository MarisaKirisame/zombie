#include "zombie/zombie.hpp"

IMPORT_ZOMBIE(default_config)

template<typename X>
struct ListNode;

template<typename X>
struct ConsNode {
  Zombie<X> x;
  Zombie<ListNode<X>> xs;
};

template<typename X>
struct ListNode {
  std::optional<ConsNode<X>> c;
};

template<>
struct GetSize<int> {
  size_t operator()(const int&) {
    return sizeof(int);
  }
};

template<typename T>
struct GetSize<ListNode<T>> {
  size_t operator()(const ListNode<T>& x) {
    return GetSize<std::optional<ConsNode<T>>>()(x.c);
  }
};

template<typename T>
struct GetSize<ConsNode<T>> {
  size_t operator()(const ConsNode<T>& x) {
    return GetSize<Zombie<T>>()(x.x) + GetSize<Zombie<ListNode<T>>>()(x.xs);
  }
};

template<typename T>
struct GetSize<std::optional<T>> {
  size_t operator()(const std::optional<T>& x) {
    return sizeof(void*) + (x ? GetSize<T>()(*x) : 0);
  }
};

int main() {
  Zombie<ListNode<int>> z {ListNode<int>()};
}
