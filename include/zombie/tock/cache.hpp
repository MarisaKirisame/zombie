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

}; // end of namespace TockTreeCaches