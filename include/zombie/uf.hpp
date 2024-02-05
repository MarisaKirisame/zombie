#pragma once

#include <memory>

template<typename T>
struct UFNode : std::enable_shared_from_this<UFNode<T>> {
  std::shared_ptr<UFNode> parent;
  T t; // only meaningful when parent.get() == nullptr
  std::shared_ptr<UFNode> get_root() {
    if (parent == nullptr) {
      return this->shared_from_this();
    } else {
      parent = parent->get_root();
      assert(parent->parent == nullptr);
      return parent;
    }
  }
  void merge(std::shared_ptr<UFNode>& _rhs) {
    auto lhs = get_root(), rhs = _rhs->get_root();
    if (lhs != rhs) {
      lhs->parent = rhs;
      lhs->t += rhs->t;
    }
  }
  explicit UFNode(const T& t) : t(t) { }
  explicit UFNode(T&& t) : t(std::move(t)) { }
  void increase(const T& t) {
    get_root()->t += t;
  }
  T value() {
    return get_root()->t;
  }
};

template<typename T>
struct UF {
  std::shared_ptr<UFNode<T>> ptr;
  explicit UF(const T& t) : ptr(std::make_shared<UFNode<T>>(t)) { }
  explicit UF(T&& t) : ptr(std::make_shared<UFNode<T>>(std::move(t))) { }
  void increase(const T& t) { ptr->increase(t); }
  void merge(UF& rhs) { ptr->merge(rhs.ptr); }
  T value() { return ptr->value(); }
  bool operator <(const UF& rhs) const {
    return ptr->get_root() < rhs.ptr->get_root();
  }
};
