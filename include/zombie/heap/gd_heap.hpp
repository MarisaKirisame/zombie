template<const ZombieConfig& cfg,
         typename T,
         typename NHIC = NotifyHeapIndexChanged<T>,
         typename NHER = NotifyHeapElementRemoved<T>>
struct GDHeap {
  struct Node {
    T t;
    cost_t cost;
    cost_t L_;
    bool operator<(const Node& r) const {
      return cost + L_ < r.cost + r.L_;
    }
  };
  cost_t L;

  struct NHIC_INNER {
    NHIC nhic;
    NHIC_INNER(const NHIC& nhic) : nhic(nhic) { }
    void operator()(const Node& n, size_t idx) {
      nhic(n.t, idx);
    }
  };

  struct NHER_INNER {
    NHER nher;
    NHER_INNER(const NHER& nher) : nher(nher) { }
    void operator()(const Node& n) {
      nher(n.t);
    }
  };

  MinHeap<Node, std::less<Node>, NHIC_INNER, NHER_INNER> heap;

  GDHeap(const NHIC& nhic = NHIC(), const NHER& nher = NHER()) : heap(std::less<Node>(), NHIC_INNER(nhic), NHER_INNER(nher)) { }

  T adjust_pop(const std::function<cost_t(const T&)> cost_f) {
    while (true) {
      assert(!heap.empty());
      Node n = heap.pop();
      cost_t new_cost = cost_f(n.t);
      if (n.cost / cfg.approx_factor.first <= new_cost / cfg.approx_factor.second &&
          new_cost / cfg.approx_factor.first <= n.cost / cfg.approx_factor.second) {
        L = n.cost + n.L_;
        return std::move(n.t);
      } else {
        n.cost = new_cost;
        heap.push(std::move(n));
      }
    }
  }

  bool empty() const {
    return heap.empty();
  }

  size_t size() const {
    return heap.size();
  }

  void push(T&& t, const cost_t& cost) {
    heap.push(Node {std::move(t), cost, L});
  }

  void touch(size_t idx) {
    heap[idx].L_ = L;
    heap.rebalance(idx, true);
    heap.notify_changed(idx);
  }
};
