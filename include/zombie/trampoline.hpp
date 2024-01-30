namespace Trampoline {

struct EOutputNode {
  virtual ~EOutputNode() { }
};

using EOutput = std::shared_ptr<EOutputNode>;

template<typename T>
struct OutputNode;

template<typename T>
using Output = std::shared_ptr<OutputNode<T>>;

template<typename T>
  struct OutputNode : EOutputNode {
  virtual bool is_return() const = 0;
  virtual const T& from_return() const = 0;
  virtual const std::function<Output<T>()>& from_tailcall() const = 0;
};

template<typename T>
struct ReturnNode : OutputNode<T> {
  T t;
  ReturnNode(T&& t) : t(std::move(t)) { }
  ReturnNode(const T& t) : t(t) { }
  bool is_return() const override {
    return true;
  }
  const T& from_return() const override {
    return t;
  }
  const std::function<Output<T>()>& from_tailcall() const override {
    assert(false);
  }
};

template<typename T>
struct TCNode : OutputNode<T> {
  // can only be called once.
  std::function<Output<T>()> func;
  template<typename... Arg>
  explicit TCNode(Arg... arg) : func(std::forward<Arg>(arg)...) { }
  bool is_return() const override {
    return false;
  }
  const T& from_return() const override {
    assert(false);
  }
  const std::function<Output<T>()>& from_tailcall() const override {
    return func;
  }
};

} // end of namespace TailCall
