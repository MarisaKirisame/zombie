#pragma once

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <cassert>
#include <functional>
#include <memory>

#include "common.hpp"

template<typename K, typename V>
struct SplayList {
  struct Node {
    K k;
    V v;

    Node* parent;
    Node* children;

    Node* splay_parent;
    mutable std::array<std::unique_ptr<Node>, 2> splay_children;

    Node(const K& k, const V& v, Node* parent, Node* children, Node* splay_parent) :
      k(k), v(v), parent(parent), children(children), splay_parent(splay_parent) {
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
      return splay_parent->splay_children[0].get() == this ? 0 : 1;
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

    std::unique_ptr<Node>& self_ptr(SplayList& tl) {
      if (splay_parent != nullptr) {
        return splay_parent->splay_children[idx_at_parent()];
      } else {
        assert(this == tl.root_node.get());
        return tl.root_node;
      }
    }

    void maintain_parent(size_t idx) {
      if (splay_children[idx] != nullptr) {
        splay_children[idx]->splay_parent = this;
      }
    }

    void swap_children(size_t idx, Node* node, size_t nidx) {
      std::swap(splay_children[idx], node->splay_children[nidx]);
      maintain_parent(idx);
      node->maintain_parent(nidx);
    }

    void remove(SplayList& tl) {
      if (parent != nullptr) {
        parent->children = children;
      }
      if (children != nullptr) {
        children->parent = parent;
      }
      if (splay_children[0].get() == nullptr) {
        splay_children[0].swap(splay_children[1]);
      } else if (splay_children[1].get() != nullptr) {
        SplayList tmp(std::move(splay_children[0]));
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

    void rotate(SplayList& tl) {
      assert(splay_parent != nullptr);
      auto splay_parent_backup = splay_parent;
      size_t idx = idx_at_parent();
      if (splay_parent->splay_parent != nullptr) {
        splay_parent->splay_parent->swap_children(splay_parent->idx_at_parent(), splay_parent, idx);
      } else {
        assert(splay_parent == tl.root_node.get());
        tl.root_node.swap(splay_parent->splay_children[idx]);
        splay_parent = nullptr;
      }
      splay_parent_backup->swap_children(idx, this, idx^1);
    }

    void splay(SplayList& tl) {
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

  // for insert
  // you have to insert the new node at the leaf before splay
  Node* find_node_without_splay(const K& k) {
    Node* last_ptr = nullptr;
    Node* ptr = root_node.get();
    while (ptr != nullptr) {
      if (ptr->k == k) {
        return ptr;
      } else {
        size_t idx = k < ptr->k ? 0 : 1;
        last_ptr = ptr;
        ptr = ptr->splay_children[idx].get();
      }
    }
    return last_ptr;
  }

  // return largest node <= k, or smallest node >= k.
  // note that when matching value found the two case collapse into one.
  // on empty splay tree return nulllptr.
  Node* find_node(const K& k) {
    Node* ret = find_node_without_splay(k);
    ret->splay(*this);
    return ret;
  }

  // find the largest Node with start <= t.
  Node* find_smaller_node(const K& k) {
    Node* ptr = find_node(k);
    if (ptr != nullptr && ptr->k > k) {
      return ptr->parent;
    } else {
      return ptr;
    }
  }

  Node* find_precise_node(const K& k) {
    Node* ptr = find_node(k);
    return (ptr == nullptr || ptr->k != k) ? nullptr : ptr;
  }

  V* find(const K& k) {
    Node* ptr = find_smaller_node(k);
    return ptr == nullptr ? nullptr : &(ptr->v);
  }

  V* find_precise(const K& k) {
    Node* ptr = find_precise_node(k);
    return ptr == nullptr ? nullptr : &(ptr->v);
  }

  bool has(const K& k) {
    Node* ptr = find_smaller_node(k);
    return ptr != nullptr;
  }

  bool has_precise(const K& k) {
    Node* ptr = find_node(k);
    return ptr != nullptr && ptr->k == k;
  }

  void remove(const K& k) {
    Node* ptr = find_smaller_node(k);
    if (ptr != nullptr) {
      ptr->remove(*this);
    }
  }

  void remove_precise(const K& k) {
    Node* ptr = find_precise_node(k);
    if (ptr != nullptr) {
      ptr->remove(*this);
    }
  }

  void insert(const K& k, const V& v) {
    Node* ptr = find_node_without_splay(k);
    if (ptr != nullptr) {
      if (ptr->k < k) {
        assert(ptr->splay_children[1].get() == nullptr);
        ptr->splay_children[1] = std::make_unique<Node>(k, v, ptr->parent, ptr, ptr);
      } else if (k < ptr->k) {
        assert(ptr->splay_children[0].get() == nullptr);
        ptr->splay_children[0] = std::make_unique<Node>(k, v, ptr, ptr->children, ptr);
      } else {
        assert (ptr->k == k);
        ptr->v = v;
      }

      ptr->splay(*this);
    } else {
      root_node = std::make_unique<Node>(k, v, nullptr, nullptr, nullptr);
    }
  }

  void debug_display_splay_helper(Node* node, int indent) {
    for (int i = 0; i < indent; i++) {
      putchar(' ');
    }

    if (node == nullptr) {
      puts("null");
    } else {
      printf("%d\n", node->k);
      debug_display_splay_helper(node->splay_children[0].get(), indent + 2);
      debug_display_splay_helper(node->splay_children[1].get(), indent + 2);
    }
  }

  void debug_display_splay() {
    debug_display_splay_helper(root_node.get(), 0);
  }
};
