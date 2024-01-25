#pragma once

#include <iostream>
#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

#include "common.hpp"

namespace TockTreeCaches {

template <typename Node> struct NoneCache {
  std::optional<std::shared_ptr<Node>> get(const Tock &t) {
    return {};
  }

  void update(const Tock &t, const std::shared_ptr<Node> &p) {
  }
};

template <typename Node> struct HashCache {
  struct HashTock {
    std::size_t operator()(const Tock &s) const {
      return std::hash<int64_t>{}(s.tock);
    }
  };

  std::unordered_map<Tock, std::weak_ptr<Node>, HashTock> hash;

  std::optional<std::shared_ptr<Node>> get(const Tock &t) {
    auto iter = hash.find(t);
    if (iter == hash.end()) {
      return {};
    } else {
      std::weak_ptr<Node> ptr = iter->second;
      if (ptr.expired()) {
        return {};
      } else {
        return ptr.lock();
      }
    }
  }

  void update(const Tock &t, std::shared_ptr<Node> &p) {
    hash[t] = std::weak_ptr<Node>(p);
  }
};

template <typename Node> struct SplayCache {
  struct SplayNode {
    std::shared_ptr<SplayNode> fa, son[2];
    std::weak_ptr<Node> value;
    Tock key;

    SplayNode(
      const std::shared_ptr<SplayNode>& fa, 
      const std::weak_ptr<Node>& value,
      const Tock& key
    ) : fa(fa), value(value), key(key) {}
  };
  
  using Pointer = std::shared_ptr<SplayNode>;

  bool which_son_of_fa(const Pointer& o) {
    assert(o->fa != nullptr);
    // o->fa->son[which_son_of_fa(o)] == o;
    return o->fa->son[1] == o;
  }

  void rotate(const Pointer& o) {
    assert(o->fa != nullptr);
    int d = which_son_of_fa(o);

    Pointer o_fa = o->fa;
    o->fa = o_fa->fa;

    if (o_fa->fa != nullptr) {
      int fd = which_son_of_fa(o_fa);
      o_fa->fa->son[fd] = o;
    }

    o_fa->son[d] = o->son[d ^ 1];
    o->son[d ^ 1] = o_fa;
  }

  void splay(const Pointer& o) {
    while (o->fa != nullptr) {
      if (o->fa->fa != nullptr) {
        if (which_son_of_fa(o) == which_son_of_fa(o->fa)) {
          rotate(o->fa);
        } else {
          rotate(o);
        }
      }

      rotate(o);
    }
  }

  Pointer find_nearby(const Pointer& root, const Tock& key) {
    Pointer o = root;
    Pointer last = nullptr;

    for (; o != nullptr; o = o->son[key < o->key ? 0 : 1]) {
      last = o;
      if (o->key == key) {
        break;
      }
    }

    if (last != nullptr) {
      splay(last);
    }
    return last;
  }

  Pointer find(const Pointer& root, const Tock& key) {
    Pointer o = find_nearby(root, key);
    return (o == nullptr || o->key == key) ? nullptr : o;
  }

  Pointer insert(const Pointer& root, const Tock& key, const std::shared_ptr<Node>& _value) {
    std::weak_ptr<Node> value(_value);

    if (root == nullptr) {
      return std::make_shared<SplayNode>(nullptr, value, key);
    }

    Pointer o = root;
    while (true) {
      if (o->key == key) {
        o->value = value;
        break;
      }

      int d = key < o->key ? 0 : 1;
      if (o->son[d] == nullptr) {
        o->son[d] = std::make_shared<SplayNode>(o, value, key);
        break;
      }

      o = o->son[key < o->key ? 0 : 1];
    }
    splay(o);
    return o;
  }

  Pointer find_max(const Pointer& root) {
    Pointer o = root;
    while (o->son[1] != nullptr) {
      o = o->son[1];
    }

    return o;
  }

  Pointer merge(const Pointer &left, const Pointer &right) {
    if (left == nullptr) return right;
    if (right == nullptr) return left;

    Pointer left_max = find_max(left);
    splay(left_max);

    left_max->son[1] = right;
    return left_max;
  }

  Pointer remove(const Pointer& o) {
    assert(o != nullptr);
    splay(o);
    for (int d = 0; d <= 1; d++) {
      if (o->son[d] != nullptr) {
        o->son[d]->fa = nullptr;
      }
    }

    return merge(o->son[0], o->son[1]);
  }

  // splay tree codes end here

  Pointer root = nullptr;

  std::optional<std::shared_ptr<Node>> get(const Tock &t) {
    auto ptr = find(root, t);

    if (ptr == nullptr) {
      return {};
    } else {
      if (ptr->value.expired()) {
        remove(ptr);
        return {};
      } else {
        return ptr->value.lock();
      }
    }
  }

  void update(const Tock &t, std::shared_ptr<Node> &p) {
    root = insert(root, t, p);
  }
};

}; // end of namespace TockTreeCaches
