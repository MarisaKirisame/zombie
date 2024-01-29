#include <algorithm>
#include <cstdio>
#include <gtest/gtest.h>
#include <memory>
#include <utility>

#include "common.hpp"
#include "zombie/tock/splay_list.hpp"

template<typename Splay, typename Node, bool is_unique>
void SplayTest() {
  using E = Element<is_unique>;

  // keys are unqiue
  std::vector<int> keys = {15, 4, 10, 34, 28, 72, 12, 38, 97, 54, 16, 75, 63, 32, 71, 65, 64, 36, 76, 25};
  std::vector<int> values = {12, 28, 20, 3, 4, 11, 19, 22, 19, 7, 7, 0, 27, 1, 18, 12, 2, 15, 6, 27};
  std::vector<int> ids;

  for (int i = 0; i < keys.size(); i++) {
    ids.push_back(i);
  }

  Splay splay;

  for (int i = 0; i < keys.size(); i++) {
    splay.insert(keys[i], E{i});
  }

  auto rng = std::default_random_engine {};
  std::shuffle(ids.begin(), ids.end(), rng);
  for (int i : ids) {
    EXPECT_TRUE(splay.has_precise(keys[i]));
  }

  for (int i = 0; i < keys.size(); i++) {
    splay.insert(keys[i], E{values[i]});
  }

  std::shuffle(ids.begin(), ids.end(), rng);
  for (int i : ids) {
    EXPECT_EQ(splay.find_precise(keys[i])->get(), values[i]);
  }

  std::vector<std::pair<int, int>> data;
  for (int i = 0; i < keys.size(); i++) {
    data.push_back({-keys[i], values[i]});
  }

  std::sort(data.begin(), data.end());
  // all the values and keys are in (0, 100)
  for (int i = 0; i < 100; i++) {
    auto it = std::lower_bound(data.begin(), data.end(), std::make_pair(-i, 0));
    auto node = splay.find_smaller_node(i);

    if (it == data.end()) {
      EXPECT_EQ(node, nullptr);
    } else {
      EXPECT_EQ(node->v.get(), it->second);
    }
  }

  std::shuffle(ids.begin(), ids.end(), rng);
  for (int i = 0; i < ids.size(); i++) {
    splay.remove_precise(keys[ids[i]]);
    EXPECT_FALSE(splay.has_precise(keys[ids[i]]));
    for (int j = i + 1; j < ids.size(); j++) {
      EXPECT_EQ(splay.find_precise(keys[ids[j]])->get(), values[ids[j]]);
    }
  }
}

TEST(SplayTest, Normal) {
  using Splay = SplayList<int, Element<false>>;
  SplayTest<Splay, Splay::Node, false>();
}

/*
TEST(SplayTest, UniqueElement) {
  using Splay = SplayList<int, Element<true>>;
  SplayTest<Splay, Splay::Node, true>();
}
*/