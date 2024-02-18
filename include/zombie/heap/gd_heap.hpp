template<const ZombieConfig& cfg,
         typename T,
         typename NHIC = NotifyHeapIndexChanged<T>,
         typename NHER = NotifyHeapElementRemoved<T>>
struct GDHeap {
  static constexpr bool staleness = false;

  struct Node {
    T t;
    cost_t cost;
    cost_t L_;
    bool operator<(const Node& r) const {
      return cost + L_ < r.cost + r.L_;
    }
    Node(T&&t_, cost_t cost_, cost_t L__) : t(std::move(t_)), cost(cost_), L_(L__) { }
    Node(Node&&) = default;
    Node& operator=(Node&&) = default;
  };
  cost_t L = 0;

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
  std::vector<Node> waiting;

  GDHeap(const NHIC& nhic = NHIC(), const NHER& nher = NHER()) : heap(std::less<Node>(), NHIC_INNER(nhic), NHER_INNER(nher)) { }

  void readjust() {
    if (!staleness) {
      if (waiting.size() * waiting.size() > heap.size()) {
        if (log_info) {
          std::cout << "readjust! " << std::endl; 
        }
        for (Node& n: waiting) {
          heap.push(std::move(n));
        }
        waiting.clear();
      }
    }
  }

  T adjust_pop(const std::function<cost_t(const T&)> cost_f) {
    while (true) {
      assert(!heap.empty());
      Node n = heap.pop();
      cost_t new_cost = cost_f(n.t);
      //if (n.cost / cfg.approx_factor.first <= new_cost / cfg.approx_factor.second &&
      //    new_cost / cfg.approx_factor.first <= n.cost / cfg.approx_factor.second) {
      if (n.cost == new_cost) {
        if (log_info) {
          std::cout << "popped " << n.cost << ", " << n.L_ << std::endl;
        }
        if (staleness) {
          L = std::max(L, n.cost + n.L_);
        } else {
          readjust();
        }
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
    return heap.size() + waiting.size();
  }

  size_t insert_count = 0;

  void push(T&& t, const cost_t& cost) {
    if (staleness) {
      // in classical gd or gdsf, L is updated at pop.
      // this create a very long warmup phase.
      // doing it here avoid the warmup.
      // weird. doesnt work.
      // L += cost;
      // std::cout << "L is: " << L << std::endl;
      heap.push(Node(std::move(t), cost, L));
    } else {
      waiting.push_back(Node(std::move(t), cost, L));
      readjust();
    }
  }

  void touch(size_t idx) {
    heap[idx].L_ = L;
    heap.rebalance(idx, true);
    heap.notify_changed(idx);
  }
};
