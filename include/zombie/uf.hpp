#pragma once

#include <memory>
#include <vector>
#include <random>
#include <unordered_set>

template<typename T>
struct UFNode : std::enable_shared_from_this<UFNode<T>> {
  T& get_largest() {
    static T largest(0);
    return largest;
  }

  // parent < shared_from_this()
  mutable std::shared_ptr<UFNode> parent;

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

  std::shared_ptr<const UFNode> get_root() const {
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
      if (lhs < rhs) {
        rhs->parent = lhs;
        lhs->t += rhs->t;
        if (lhs->t > get_largest()) {
          get_largest() = lhs->t;
          std::cout << "new largest uf: " << get_largest() << std::endl;
        }
      } else if (lhs > rhs) {
        lhs->parent = rhs;
        rhs->t += lhs->t;
        if (rhs->t > get_largest()) {
          get_largest() = rhs->t;
          std::cout << "new largest uf: " << get_largest() << std::endl;
        }
      }
    }
  }

  explicit UFNode(const T& t) : t(t) { }
  explicit UFNode(T&& t) : t(std::move(t)) { }
  ~UFNode() { }

  T value() const {
    return get_root()->t;
  }

  template<typename F>
  void update(const F& f) {
    auto root = get_root();
    root->t = f(root->t);
  }
};

template<typename T>
struct UF {
  std::shared_ptr<UFNode<T>> ptr;
  explicit UF(const T& t) : ptr(std::make_shared<UFNode<T>>(t)) { }
  explicit UF(T&& t) : ptr(std::make_shared<UFNode<T>>(std::move(t))) { }
  void merge(UF& rhs) { ptr->merge(rhs.ptr); }
  T value() const { return ptr->value(); }
  bool operator <(const UF& rhs) const {
    return ptr->get_root() < rhs.ptr->get_root();
  }
  bool operator ==(const UF& rhs) const {
    return ptr->get_root() == rhs.ptr->get_root();
  }
  template<typename F>
  void update(const F& f) {
    return ptr->update(f);
  }
};

template <typename T>
struct std::hash<UF<T>> {
  std::size_t operator()(const UF<T>& t) const {
    return std::hash<std::shared_ptr<UFNode<T>>>()(t.ptr->get_root());
  }
};

// normal set cannot store UF, as the UF may merge and become equal.
// this data structure allow change and additionally compact and remove duplicate UF.
template<typename T>
struct UFSet {
  mutable std::vector<UF<T>> data;

  void fixup(size_t idx) const {
    while (idx != 0 && idx < data.size()) {
      size_t parent_idx = ((idx+1)/2)-1;
      if (data[idx] < data[parent_idx]) {
        std::swap(data[idx], data[parent_idx]);
        idx = parent_idx;
      } else if (data[idx] == data[parent_idx]) {
        std::swap(data[idx], data.back());
        data.pop_back();
      } else {
        return;
      }
    }
  }


  void insert(const UF<T>& uf) {
    data.push_back(uf);

    // alright, it is super unclear what's the right amount of fixing we should do.
    // it even look like we can do no fixing at all, using the data member as a raw array,
    // and only fix when we are in cost().
    //
    // in the end I decide to do a log amount of work here,
    // without any recursive fixing.
    // my intuition is that the hot node(merged a lot, so a more possible hit) is up top,
    // so we want to populate up there.

    fixup(data.size() - 1);
    // additionally, normal binary heap fixup is very unfair,
    // and will starve the high-level node for a long time
    // to fix this we also redo random fixup somewhere else.
    std::default_random_engine re;
    std::uniform_int_distribution<int> uniform_dist(0, data.size() - 1);
    fixup(uniform_dist(re));
  }

  T sum() const {
    std::unordered_set<UF<T>> counted;
    return sum(counted);
  }

  T sum(std::unordered_set<UF<T>>& counted) const {
    T result(0);
    std::vector<UF<T>> new_data;
    for (const UF<T>& uf: data) {
      if (counted.insert(uf).second) {
        result += uf.value();
        new_data.push_back(uf);
      }
    }
    data = std::move(new_data);
    return result;
  }

  void merge(UF<T>& with) const {
    for (UF<T>& uf : data) {
      uf.merge(with);
    }
    data.clear();
    data.push_back(with);
  }
};
