#pragma once

#include <vector>
#include <queue>
#include <tuple>

template<typename T>
struct Node {
    double cost;
    double offset;
    T value;
};

template<typename T>
struct NodeCompare
{
    bool operator()(const Node<T>& left, const Node<T>& right) const
    {
        return (left.cost + left.offset) > (right.cost + right.offset);
    }
};

template <typename T>
struct GdsHeap {
    std::priority_queue<Node<T>, std::vector<Node<T>>, NodeCompare<T>> queue;
    double L = 0.0;

    bool empty() const {
        return queue.empty();
    }

    size_t size() const {
        return queue.size();
    }

    void push(const double cost, const T& value) {
        queue.push(Node<T> {cost, L, value});
    }

    void push_raw(const Node<T> &node) {
        queue.push(node);
    }

    const Node<T> &top() const {
        return queue.top();
    }

    void pop_raw() {
        queue.pop();
    }

    void pop() {
        const auto &t = top();
        L = t.cost + t.offset;
        queue.pop();
    }
};