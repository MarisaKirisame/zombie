#pragma once

#include <iostream>
#include <cassert>
#include <functional>
#include <memory>

#include "common.hpp"

namespace TockTreeImpls {
namespace CPS {

// under CPS mode there is no recursive structure, only a list.
// of course, list dont have random access so we overlay a splay tree on top.
template<typename V, typename NotifyParentChanged>
struct TockList {
  struct Node {
    V data;
    Tock start;

    Node* parent;
    Node* children;

    Node* splay_parent;
    mutable std::array<std::unique_ptr<Node>, 2> splay_children;

    Node(const V& data, const Tock& start, Node* parent, Node* children, Node* splay_parent) :
      data(data), start(start), parent(parent), children(children), splay_parent(splay_parent) {
      if (parent != nullptr) {
        assert(parent->children == children);
        parent->children = this;
      }
      if (children != nullptr) {
        assert(children->parent == parent);
        children->parent = this;
      }
    }
    size_t idx_at_parent() const {
      assert(splay_parent != nullptr);
      return &(splay_parent->son[0]) == this ? 0 : 1;
    }

    Node* max_node() {
      Node* ptr = this;
      while (ptr->splay_children[1].get() != nullptr) {
        ptr = ptr->splay_children[1].get();
      }
      return ptr;
    }

    Node* min_node() {
      Node* ptr = this;
      while (ptr->splay_children[0].get() != nullptr) {
        ptr = ptr->splay_children[0].get();
      }
      return ptr;
    }

    std::unique_ptr<Node>& self_ptr(TockList& tl) {
      if (splay_parent != nullptr) {
        return splay_parent->splay_children[idx_at_parent()];
      } else {
        assert(this == tl.root_node.get());
        return tl.root_node;
      }
    }

    std::unique_ptr<Node> swap_children(size_t idx, std::unique_ptr<Node>&& node) {
      auto ret = splay_children[idx].swap(std::move(node));
      std::swap(splay_children[idx]->parent, ret->parent);
      return ret;
    }

    void remove(TockList& tl) {
      if (parent != nullptr) {
        parent->children = children;
      }
      if (children != nullptr) {
        children->parent = parent;
      }
      if (splay_children[0].get() == nullptr) {
        splay_children[0].swap(splay_children[1]);
      } else if (splay_children[1].get() != nullptr) {
        TockList tmp(std::move(splay_children[0]));
        Node* left = tmp.root_node->max_node();
        left->splay(tmp);
        assert(left->splay_children[1].get() == nullptr);
        left->splay_children[1].swap(splay_children[1]);
        assert(left->parent == nullptr);
        splay_children[0] = std::move(tmp.root_node);
      }
      assert(splay_children[1].get() == nullptr);
      if (splay_children[0].get() == nullptr) {
        self_ptr(tl).reset();
      } else {
        self_ptr(tl).swap(splay_children[0]);
      }
    }

    void rotate(TockList& tl) {
      assert(splay_parent != nullptr);
      size_t idx = idx_at_parent();
      if (splay_parent->splay_parent != nullptr) {
        splay_parent->splay_parent->swap_children(splay_parent->idx_at_parent(), splay_parent->splay_children[idx]);
      } else {
        assert(splay_parent == tl.root_node.get());
        tl.root_node.swap(splay_parent->splay_children[idx]);
        splay_parent = nullptr;
      }
      splay_parent->swap_children(idx, splay_children[idx^1]);
    }

    void splay(TockList& tl) {
      while (splay_parent != nullptr) {
        if (splay_parent->splay_parent != nullptr) {
          if (idx_at_parent() == splay_parent->idx_at_parent()) {
            splay_parent->rotate(tl);
          } else {
            rotate(tl);
          }
        }
        rotate(tl);
      }
    }

  };
  mutable std::unique_ptr<Node> root_node;

  // return largest node <= t, or smallest node >= t.
  // note that when matching value found the two case collapse into one.
  // on empty splay tree return nulllptr.
  Node* find_node(const Tock& t) {
    Node* last_ptr = nullptr;
    Node* ptr = root_node.get();
    while (ptr != nullptr) {
      if (ptr->start == t) {
        return &(ptr->data);
      } else {
        size_t idx = ptr->start < t ? 0 : 1;
        last_ptr = ptr;
        ptr = ptr->splay_children[idx];
      }
    }
    if (last_ptr != nullptr) {
      last_ptr->splay(*this);
    }
    return last_ptr;
  }

  // find the largest Node with start <= t.
  Node* find_smaller_node(const Tock& t) {
    Node* ptr = find_node(t);
    if (ptr != nullptr && ptr->start > t) {
      return ptr->parent;
    } else {
      return ptr;
    }
  }

  Node* find_precise_node(const Tock& t) {
    Node* ptr = find_node(t);
    return (ptr == nullptr || ptr->start != t) ? nullptr : ptr;
  }

  V* find(const Tock& t) {
    Node* ptr = find_smaller_node(t);
    return ptr == nullptr ? nullptr : &(ptr->data);
  }

  V* find_precise(const Tock& t) {
    Node* ptr = find_precise_node(t);
    return ptr == nullptr ? nullptr : &(ptr->data);
  }

  bool has_precise(const Tock& t) {
    Node* ptr = find_node(t);
    return ptr != nullptr && ptr->start == t;
  }

  void remove(const Tock& t) {
    Node* ptr = find_smaller_node(t);
    if (ptr != nullptr) {
      ptr->remove(*this);
    }
  }

  void remove_precise(const Tock& t) {
    Node* ptr = find_precise_node(t);
    if (ptr != nullptr) {
      ptr->remove(*this);
    }
  }

  void insert(const Tock& t, const V& v) {
    Node* ptr = find_node();
    if (ptr != nullptr) {
      if (ptr->start < t) {
        assert(ptr->splay_children[0].get() == nullptr);
        ptr->splay_children[0] = std::make_unique<Node>(t, v, ptr->parent, ptr, ptr);
      } else if (ptr->start > t) {
        assert(ptr->splay_children[1].get() == nullptr);
        ptr->splay_children[0] = std::make_unique<Node>(t, v, ptr, ptr->children, ptr);
      } else {
        assert (ptr->start == t);
        ptr->data = v;
      }
    } else {
      root_node = std::make_unique<Node>(t, v, nullptr, nullptr, nullptr);
    }
  }
};

}; // end of namespace CPS
}; // end of namespace TockTreeImpls
