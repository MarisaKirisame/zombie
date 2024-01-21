namespace Trampoline {

struct EOutputNode {
  virtual ~EOutputNode() { }
};

using EOutput = std::shared_ptr<EOutputNode>;

template<typename T>
struct OutputNode : EOutputNode { };

template<typename T>
using Output = std::shared_ptr<OutputNode<T>>;

template<typename T>
struct ReturnNode : OutputNode<T> {
  T t;
  ReturnNode(T&& t) : t(std::move(t)) { }
  ReturnNode(const T& t) : t(t) { }
};

template<typename T>
struct TCNode : OutputNode<T> {
  // can only be called once.
  std::function<Output<T>()> func;
  template<typename... Arg>
  explicit TCNode(Arg... arg) : func(std::forward<Arg>(arg)...) { }
};

} // end of namespace TailCall
