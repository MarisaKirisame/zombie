#include "zombie/tock/tock.hpp"

struct NotifyParentChanged
{
  void operator()(const TockTreeData<int> &n, const TockTreeData<int> *p)
  {
  }
};

int main()
{
  TockTree<TockTreeImpl::Tree, int, NotifyParentChanged> tt;
  tt.put({2, 6}, 1);
  tt.put({1, 10}, 2);
  tt.check_invariant();
  tt.get_node(5).value;
  tt.get_node(6).value;
}
