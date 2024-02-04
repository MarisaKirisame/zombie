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
    mutable std::array<Node*, 2> splay_children = {nullptr, nullptr};

    Node(const K& k, const V& v, Node* parent, Node* children, Node* splay_parent) :
      k(k), v(v), parent(parent), children(children), splay_parent(splay_parent) {
      if (parent != nullptr) {
        assert(parent->children == children);
        assert(parent->k <= k);
        parent->children = this;
      }
      if (children != nullptr) {
        assert(children->parent == parent);
        assert(children->k >= k);
        children->parent = this;
      }
    }

    size_t idx_at_parent() const {
      assert(splay_parent != nullptr);
      return splay_parent->splay_children[0] == this ? 0 : 1;
    }

    Node* max_node() {
      Node* ptr = this;
      while (ptr->splay_children[1] != nullptr) {
        ptr = ptr->splay_children[1];
      }
      return ptr;
    }

    Node* min_node() {
      Node* ptr = this;
      while (ptr->splay_children[0] != nullptr) {
        ptr = ptr->splay_children[0];
      }
      return ptr;
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

      splay(tl.root_node);
      if (splay_children[0] == nullptr && splay_children[1] == nullptr) {
        tl.root_node = nullptr;
      } else if (splay_children[0]== nullptr) {
        tl.root_node = std::move(splay_children[1]);
        tl.root_node->splay_parent = nullptr;
      } else if (splay_children[1] == nullptr) {
        tl.root_node = std::move(splay_children[0]);
        tl.root_node->splay_parent = nullptr;
      } else {
        splay_children[1]->splay_parent = nullptr;

        splay_children[1]->min_node()->splay(splay_children[1]);
        splay_children[1]->splay_children[0] = std::move(splay_children[0]);
        splay_children[1]->maintain_parent(0);
        tl.root_node = std::move(splay_children[1]);
      }

      delete this;
    }

    void rotate(Node*& root_node) {
      assert(splay_parent != nullptr);
      auto splay_parent_backup = splay_parent;
      size_t idx = idx_at_parent();
      if (splay_parent->splay_parent != nullptr) {
        splay_parent->splay_parent->swap_children(splay_parent->idx_at_parent(), splay_parent, idx);
      } else {
        assert(splay_parent == root_node);
        std::swap(root_node, splay_parent->splay_children[idx]);
        splay_parent = nullptr;
      }
      splay_parent_backup->swap_children(idx, this, idx^1);
    }

    void splay(Node*& root_node) {
      while (splay_parent != nullptr) {
        if (splay_parent->splay_parent != nullptr) {
          if (idx_at_parent() == splay_parent->idx_at_parent()) {
            splay_parent->rotate(root_node);
          } else {
            rotate(root_node);
          }
        }
        rotate(root_node);
      }

      assert(this == root_node);
    }

  };
  mutable Node* root_node = nullptr;

  ~SplayList() {
    if (root_node != nullptr) {
      for (Node* left_ptr = root_node->parent; left_ptr != nullptr;) {
        Node* to_delete = left_ptr;
        left_ptr = left_ptr->parent;
        delete to_delete;
      }
      for (Node* right_ptr = root_node->children; right_ptr != nullptr;) {
        Node* to_delete = right_ptr;
        right_ptr = right_ptr->children;
        delete to_delete;
      }
      delete root_node;
    }
  }

  // for insert
  // you have to insert the new node at the leaf before splay
  Node* find_node_without_splay(const K& k) {
    if (root_node == nullptr) {
      return nullptr;
    }

    Node* last_ptr = nullptr;
    Node* ptr = root_node;
    while (ptr != nullptr) {
      if (ptr->k == k) {
        return ptr;
      } else {
        size_t idx = k < ptr->k ? 0 : 1;
        last_ptr = ptr;
        ptr = ptr->splay_children[idx];
      }
    }
    return last_ptr;
  }

  // return largest node <= k, or smallest node >= k.
  // note that when matching value found the two case collapse into one.
  // on empty splay tree return nulllptr.
  Node* find_node(const K& k) {
    Node* ret = find_node_without_splay(k);
    if (ret != nullptr) {
      ret->splay(this->root_node);
    }
    return ret;
  }

  // find the largest Node with start <= t.
  Node* find_le_node(const K& k) {
    Node* ptr = find_node(k);
    if (ptr == nullptr) {
      return nullptr;
    } else if (ptr->k <= k) {
      return ptr;
    } else {
      if (ptr->splay_children[0] == nullptr) {
        return nullptr;
      } else {
        auto ret = ptr->splay_children[0]->max_node();
        ret->splay(root_node);
        return ret;
      }
    }
  }

  Node* find_precise_node(const K& k) {
    Node* ptr = find_node(k);
    return (ptr == nullptr || ptr->k != k) ? nullptr : ptr;
  }

  V* find_le(const K& k) {
    Node* ptr = find_le_node(k);
    return ptr == nullptr ? nullptr : &(ptr->v);
  }

  V* find_precise(const K& k) {
    Node* ptr = find_precise_node(k);
    return ptr == nullptr ? nullptr : &(ptr->v);
  }

  bool has_le(const K& k) {
    Node* ptr = find_le_node(k);
    return ptr != nullptr;
  }

  bool has_precise(const K& k) {
    Node* ptr = find_node(k);
    return ptr != nullptr && ptr->k == k;
  }

  void remove_le(const K& k) {
    Node* ptr = find_le_node(k);
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
        assert(ptr->splay_children[1] == nullptr);
        ptr->splay_children[1] = new Node(k, v, ptr, ptr->children, ptr);
      } else if (k < ptr->k) {
        assert(ptr->splay_children[0] == nullptr);
        ptr->splay_children[0] = new Node(k, v, ptr->parent, ptr, ptr);
      } else {
        assert (ptr->k == k);
        ptr->v = v;
      }
      ptr->splay(this->root_node);
    } else {
      root_node = new Node(k, v, nullptr, nullptr, nullptr);
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
      debug_display_splay_helper(node->splay_children[0], indent + 2);
      debug_display_splay_helper(node->splay_children[1], indent + 2);
    }
  }

  void debug_display_splay() {
    debug_display_splay_helper(root_node, 0);
  }
};
