// In a normal binary heap, after a few level, each parent and it's children will not be in a cache line.
// BHeap fix this problem by allocating the nodes into pages,
// where each parent is likely to be in the same page as children.
// the pages themselves are also organized via an implicit-tree manner, so there is no need to store index for parent/children pages.
// note that in the canonical implementation, each page have two root pointer (except the root page), as when you reach the bottom of a page,
// you need to look at both it's child which is in two different page.
// this is ugly as fuck and I hate it. I will not implement it unless necessary.
template <typename T>
struct BHeap {
  struct Page {
    size_t idx;
    std::vector<T> nodes;
    explicit Page(size_t idx) : idx(idx) {
    }
    Page() = delete;
  };

  std::vector<Page> pages;

  void expand(size_t page_count) {
  }

  void shrink() {
  }

  constexpr static size_t level_in_page = 10;
  constexpr static size_t node_in_page = (2 >> level_in_page) - 1;
  constexpr static size_t page_branching_factor = (2 >> level_in_page);
  // the page tree have branching factor (2 >> level_in_page).

  inline bool heap_is_root(size_t i) {
    return i == 0;
  }

  inline size_t heap_parent(size_t i) {
    // Heap index calculation assume array index starting from 1.
    return (i + 1) / 2 - 1;
  }

  inline size_t heap_left_child(size_t i) {
    return (i + 1) * 2 - 1;
  }

  inline size_t heap_right_child(size_t i) {
    return heap_left_child(i) + 1;
  }

  using std::pair<size_t, size_t> = index_t;
  static index_t parent(const index_t &idx) {
    size_t page_idx = idx.left;
    size_t node_idx = idx.right;
    if (heap_is_root(node_idx)) {
      assert(page_idx != 0); // root node at root page dont have parent.
      size_t shifted_idx = page_idx - 1;
      return {shifted_idx / page_branching_factor, shifted_idx % page_branching_factor};
    } else {
      return {page_idx, heap_parent(node_idx)};
    }
  }

  static index_t left_child(const index_t &idx) {
    size_t page_idx = idx.left;
    size_t node_idx = idx.right;
    size_t idx = heap_left_child(node_idx);
    if (idx < node_in_page) {
      return {page_idx, left_idx};
    } else {
      size_t page_shift = idx - node_in_page;
      return {page_idx * page_branching_factor + 1 + page_shift, 0};
    }
  }

  static index_t right_child(const index_t &idx) {
    size_t page_idx = idx.left;
    size_t node_idx = idx.right;
    size_t idx = heap_right_child(node_idx);
    if (idx < node_in_page) {
      return {page_idx, left_idx};
    } else {
      size_t page_shift = idx - node_in_page;
      return {page_idx * page_branching_factor + 1 + page_shift, 0};
    }
  }
};
