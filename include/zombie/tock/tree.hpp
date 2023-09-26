#pragma once

#include <iostream>
#include <cassert>
#include <functional>

#include "common.hpp"
#include "../common.hpp" // for Space

namespace TockTreeImpls {

template<typename V, typename NotifyParentChanged>
struct TockTree {
private:
  struct Node {
    // nullptr iff toplevel
    Node* parent;
    TockTreeData<V> data;
    std::map<Tock, Node> children;

    Node(Node* parent, const TockRange& range, const V& value) :
      parent(parent), data{range, value} { }
    Node(Node* parent, const TockRange& range, V&& value) :
      parent(parent), data{range, std::move(value)} { }


    bool children_in_range(const Tock& t) const {
      auto it = largest_value_le(children, t);
      if (it == children.end()) {
        return false;
      } else {
        assert(it->second.data.range.beg <= t);
        return t < it->second.data.range.end;
      }
    }

    const Node& get_shallow(const Tock& t) const {
      auto it = largest_value_le(children, t);
      assert(it != children.end());
      assert(it->second.data.range.beg <= t);
      assert(t < it->second.data.range.end);
      return it->second;
    }

    Node& get_shallow(const Tock& t) {
      auto it = largest_value_le(children, t);
      assert(it != children.end());
      assert(it->second.data.range.beg <= t);
      assert(t < it->second.data.range.end);
      return it->second;
    }

    const Node& get_node(const Tock& t) const {
      return children_in_range(t) ? get_shallow(t).get_node(t) : *this;
    }

    Node& get_node(const Tock& t) {
      return children_in_range(t) ? get_shallow(t).get_node(t) : *this;
    }


    void delete_node() {
      // the root node is not for deletion.
      assert(parent != nullptr);
      std::map<Tock, Node>& insert_to = parent->children;
      for (auto it = children.begin(); it != children.end();) {
        auto nh = children.extract(it++);
        nh.mapped().parent = parent;
        auto new_it = insert_to.insert(std::move(nh));
        notify(new_it.position->second);
      }
      parent->children.erase(data.range.beg);
    }


    void filter_children(std::function<bool(const TockTreeData<V>)> f) {
      for (auto it = children.begin(); it != children.end();) {
        if (f(it->second.data))
          it = children.erase(it);
        else
          ++it;
      }
    }


    void check_invariant() const {
      std::optional<TockRange> prev_range;
      for (auto p : children) {
        const auto& curr_range = p.second.data.range;
        assert(range_ok(curr_range));
        assert(range_dominate(data.range, curr_range));
        if (prev_range.has_value()) {
          assert(range_nointersect(prev_range.value(), curr_range));
        }
        p.second.check_invariant();
        prev_range = curr_range;
      }
    }
  };

  static void notify(const Node& n) {
    const TockTreeData<V>* parent = n.parent == nullptr ? nullptr : &n.parent->data;
    NotifyParentChanged()(n.data, parent);
  }


public:
  Node n = Node(nullptr, TockRange{std::numeric_limits<Tock>::min(), std::numeric_limits<Tock>::max()}, V());

  TockTreeData<V>& get_node(const Tock& t) {
    return n.get_node(t).data;
  }

  const TockTreeData<V>& get_node(const Tock& t) const {
    return n.get_node(t).data;
  }

  bool has_precise(const Tock& t) const {
    return get_node(t).range.beg == t;
  }

  TockTreeData<V>& get_precise_node(const Tock& t) {
    assert(has_precise(t));
    return get_node(t);
  }

  const TockTreeData<V>& get_precise_node(const Tock& t) const {
    assert(has_precise(t));
    return get_node(t);
  }


  TockTreeData<V>* get_parent(const Tock& t) {
    auto& node = n.get_node(t);
    if (! has_precise(t))
      return &node.data;
    else if (node.parent)
      return &node.parent->data;
    else
      return nullptr;
  }


  void check_invariant() const {
    n.check_invariant();
  }

  TockTreeData<V>& put(const TockRange& r, const V& v) {
    return put(r, v);
  }

  TockTreeData<V>& put(const TockRange& r, V&& v) {
    Node& n = this->n.get_node(r.beg);
    // disallow inserting the same node twice
    assert(n.data.range.beg != r.beg);
    assert(range_dominate(n.data.range, r));

    auto* inserted = &n.children;
    auto it = inserted->insert({r.beg, Node(&n, r, std::move(v))}).first;
    Node& inserted_node = it->second;
    notify(inserted_node);
    ++it;
    while (it != inserted->end() && range_dominate(r, it->second.data.range)) {
      auto nh = inserted->extract(it++);
      nh.mapped().parent = &inserted_node;
<<<<<<< HEAD
      auto new_it = inserted_node.children.insert(std::move(nh));
      notify(new_it.position->second);
=======
      inserted_node.children.insert(std::move(nh));
      notify(nh.mapped());
>>>>>>> 8943513 (save)
    }
    return inserted_node.data;
  }


  void remove_precise(const Tock& t) {
    assert(has_precise(t));
    n.get_node(t).delete_node();
  }

  void filter_children(std::function<bool(const TockTreeData<V>&)> f, const Tock& t) {
    assert(has_precise(t));
    n.get_node(t).filter_children(std::move(f));
  }
};


template<typename V, typename NPC>
std::ostream& print(std::ostream& os, const typename TockTree<V, NPC>::Node& n) {
  os << "Node " << n.data.value << " [" << n.data.range.beg << ", " << n.data.range.end << ")" << " {" << std::endl;
  for (const auto& p : n.children) {
    print<V>(os, p.second);
  }
  return os << "}" << std::endl;
}

template<typename V, typename NPC>
std::ostream& operator<<(std::ostream& os, const TockTree<V, NPC>& v) {
  print<V>(os, v.n);
  return os;
}

}; // end of namespace TockTreeImpls
