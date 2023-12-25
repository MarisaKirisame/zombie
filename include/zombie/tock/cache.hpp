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

  Pointer find(Pointer o, const Tock& key) {
    Pointer& last = o;

    for (; o != nullptr; o = o->son[key < o->key ? 0 : 1]) {
      last = o;
      if (o->key == key) {
        break;
      }
    }

    if (last != nullptr) {
      splay(last);
    }
    return o;
  }

  void insert(Pointer& o, const Tock& key, const std::shared_ptr<Node>& _value) {
    std::weak_ptr<Node> value(_value);
    Pointer fa = nullptr;

    while (o != nullptr) {
      if (o->key == key) {
        o->value = value;
        break;
      }

      fa = o;
      o = o->son[key < o->key ? 0 : 1];
    }

    if (o == nullptr) {
      o = std::make_shared<SplayNode>(fa, value, key);
    }
    splay(o);
  }

  Pointer root = nullptr;

  std::optional<std::shared_ptr<Node>> get(const Tock &t) {
    auto ptr = find(root, t);

    if (ptr == nullptr) {
      return {};
    } else {
      if (ptr->value.expired()) {
        return {};
      } else {
        return ptr->value.lock();
      }
    }
  }

  void update(const Tock &t, std::shared_ptr<Node> &p) {
    insert(root, t, p);
  }
};

}; // end of namespace TockTreeCaches