#pragma once

#include <iostream>
#include <cassert>
#include <functional>
#include <memory>

#include "common.hpp"

namespace TockTreeImpls {

// note that as TockRange is open-close, the largest value is natrually out of the range of tock tree.
// this is not a critical limitation: one can always use Option<X> in place of X, with None being the largest value.
template<typename V, template<typename> class Cache, typename NotifyParentChanged>
struct TockTree {
// todo: it should be private, but upward fixing of tailcall require more publicity then i thought.
// make some stuff private again.
public:
  struct Node : std::enable_shared_from_this<Node> {
    // nullptr iff toplevel
    std::shared_ptr<Node> parent;
    TockTreeData<V> data;
    std::map<Tock, std::shared_ptr<Node>> children;

    Node(std::shared_ptr<Node> parent, const TockRange& range, const V& value) :
      parent(parent), data{range, value} { }
    Node(std::shared_ptr<Node> parent, const TockRange& range, V&& value) :
      parent(parent), data{range, std::move(value)} { }

    void finish_tc(const Tock& t, const std::function<void(V*)>& f) {
      if (parent != nullptr) {
        assert(data.range.end == std::numeric_limits<Tock>::max());
        data.range.end = t;
        f(&data.value);
        parent->finish_tc(t, f);
      }
    }

    bool children_in_range(const Tock& t) const {
      auto it = largest_value_le(children, t);
      if (it == children.end()) {
        return false;
      } else {
        assert(it->second->data.range.beg <= t);
        return t < it->second->data.range.end;
      }
    }

    std::shared_ptr<const Node> get_shallow(const Tock& t) const {
      auto it = largest_value_le(children, t);
      assert(it != children.end());
      assert(it->second->data.range.beg <= t);
      assert(t < it->second->data.range.end);
      return it->second;
    }

    std::shared_ptr<Node> get_shallow(const Tock& t) {
      auto it = largest_value_le(children, t);
      assert(it != children.end());
      assert(it->second->data.range.beg <= t);
      assert(t < it->second->data.range.end);
      return it->second;
    }

    std::shared_ptr<const Node> get_node(const Tock& t) const {
      std::shared_ptr<const Node> cur = this->shared_from_this();

      while (true) {
        if (cur->children_in_range(t)) {
          cur = cur->get_shallow(t);
        } else if (in_range(cur->data.range, t)) {
          break;
        } else {
          cur = cur->parent;
        }
      }

      return cur;
    }

    std::shared_ptr<Node> get_node(const Tock& t) {
      std::shared_ptr<Node> cur = this->shared_from_this();

      while (true) {
        if (cur->children_in_range(t)) {
          cur = cur->get_shallow(t);
        } else if (in_range(cur->data.range, t)) {
          break;
        } else {
          cur = cur->parent;
        }
      }

      return cur;
    }

    void delete_node() {
      // the root node is not for deletion.
      assert(parent != nullptr);
      std::map<Tock, std::shared_ptr<Node>>& insert_to = parent->children;
      for (auto it = children.begin(); it != children.end();) {
        auto nh = children.extract(it++);
        nh.mapped()->parent = parent;
        auto new_it = insert_to.insert(std::move(nh));
        notify(new_it.position->second);
      }
      parent->children.erase(data.range.beg);
    }

    void filter_children(const std::function<bool(const TockTreeData<V>)>& f) {
      for (auto it = children.begin(); it != children.end();) {
        if (f(it->second->data)) {
          it = children.erase(it);
        } else {
          ++it;
        }
      }
    }

    void check_invariant() const {
      std::optional<TockRange> prev_range;
      for (auto p : children) {
        const auto& curr_range = p.second->data.range;
        assert(range_ok(curr_range));
        assert(range_dominate(data.range, curr_range));
        if (prev_range.has_value()) {
          assert(range_nointersect(prev_range.value(), curr_range));
        }
        p.second->check_invariant();
        prev_range = curr_range;
      }
    }
  };

  static void notify(std::shared_ptr<Node> n) {
    const TockTreeData<V>* parent = n->parent == nullptr ? nullptr : &n->parent->data;
    NotifyParentChanged()(n->data, parent);
  }

  mutable Cache<Node> cache;

public:
  std::shared_ptr<Node> n = std::make_shared<Node>(nullptr, TockRange{std::numeric_limits<Tock>::min(), std::numeric_limits<Tock>::max()}, V());

  std::shared_ptr<Node> visit_node(const Tock& t) {
    auto nd = cache.get(t).value_or(n)->get_node(t);
    cache.update(t, nd);
    return nd;
  }

  TockTreeData<V>& get_node(const Tock& t) {
    auto nd = cache.get(t).value_or(n)->get_node(t);
    cache.update(t, nd);
    return nd->data;
  }

  const TockTreeData<V>& get_node(const Tock& t) const {
    auto nd = cache.get(t).value_or(n)->get_node(t);
    cache.update(t, nd);
    return nd->data;
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
    auto node = n->get_node(t);
    if (! has_precise(t)) {
      return &node->data;
    } else if (node->parent) {
      return &node->parent->data;
    } else {
      return nullptr;
    }
  }

  void check_invariant() const {
    n->check_invariant();
  }

  TockTreeData<V>& put(const TockRange& r, const V& v) {
    V v_ = v;
    return put(r, std::move(v_));
  }

  TockTreeData<V>& put(const TockRange& r, V&& v) {
    std::shared_ptr<Node> n = this->n->get_node(r.beg);
    // disallow inserting the same node twice
    assert(n->data.range.beg != r.beg);
    assert(range_dominate(n->data.range, r));

    auto* inserted = &n->children;
    std::shared_ptr<Node> new_ptr = std::make_shared<Node>(n, r, std::move(v));
    auto it = inserted->insert({r.beg, new_ptr}).first;
    notify(new_ptr);
    ++it;
    while (it != inserted->end() && range_dominate(r, it->second->data.range)) {
      auto nh = inserted->extract(it++);
      nh.mapped()->parent = new_ptr;
      auto new_it = new_ptr->children.insert(std::move(nh));
      notify(new_it.position->second);
    }
    return new_ptr->data;
  }

  void remove_precise(const Tock& t) {
    assert(has_precise(t));
    visit_node(t)->delete_node();
  }

  void filter_children(const std::function<bool(const TockTreeData<V>&)>& f, const Tock& t) {
    assert(has_precise(t));
    visit_node(t)->filter_children(f);
  }
};

template<typename V, template<typename> class Cache, typename NPC>
std::ostream& print(std::ostream& os, const typename TockTree<V, Cache, NPC>::Node& n) {
  os << "Node " << n.data.value << " [" << n.data.range.beg << ", " << n.data.range.end << ")" << " {" << std::endl;
  for (const auto& p : n.children) {
    print<V>(os, p.second);
  }
  return os << "}" << std::endl;
}

template<typename V, template<typename> class Cache, typename NPC>
std::ostream& operator<<(std::ostream& os, const TockTree<V, Cache, NPC>& v) {
  print<V>(os, v.n);
  return os;
}

}; // end of namespace TockTreeImpls
