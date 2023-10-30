#pragma once

#include <vector>
#include <cstddef>

const size_t SegmentSize = 128;

template<typename T>
struct SegmentedArray {
private:
    std::vector<std::vector<T>> segments;

public:
    struct iterator {
        std::vector<std::vector<T>>* segments;
        size_t i, j;

        iterator(std::vector<std::vector<T>>* seg, size_t i, size_t j) :
            segments(seg), i(i), j(j) {}

        bool operator!=(const iterator &rhs) const {
            return i != rhs.i || j != rhs.j;
        }

        iterator& operator++() {
            if (j + 1 == SegmentSize) {
                i++; j = 0;
            } else {
                j++;
            }

            return *this;
        }

        T& operator*() {
            return (*segments)[i][j];
        }
    };

    void resize(size_t size) {
        size_t seg_size = (size + SegmentSize - 1) / SegmentSize;

        while (segments.size() < seg_size) {
            std::vector<T> _v;
            segments.push_back(std::move(_v));
            segments.back().reserve(SegmentSize)
            segments.back().resize(
                seg_size == segments.size() ? size % SegmentSize : SegmentSize
            );
        }
    }

    void push_back(T&& item) {
        if (segments.size() == 0 || segments.back().size() == SegmentSize) {
            std::vector<T> _v;
            segments.push_back(std::move(_v));
            segments.back().reserve(SegmentSize);
        }

        segments.back().push_back(std::move(item));
    }

    void pop_back() {
        segments.back().pop_back();
        if (segments.back().empty()) {
            segments.pop_back();
        }
    }

    void push_back(const T& item) {
        T t(item);
        push_back(std::move(t));
    }

    T& operator[] (size_t index) {
        return segments[index / SegmentSize][index % SegmentSize];
    }

    const T& operator[] (size_t index) const {
        return segments[index / SegmentSize][index % SegmentSize];
    }

    size_t size() const {
        if (segments.empty()) {
            return 0;
        } else {
            return (segments.size() - 1) * SegmentSize + segments.back().size();
        }
    }

    bool empty() const {
        return segments.empty();
    }

    iterator begin() {
        return iterator(&segments, 0, 0);
    }

    iterator end() {
        if (segments.empty()) {
            return iterator(&segments, 0, 0);
        } else {
            return iterator(&segments, segments.size() - 1, segments.back().size());
        }
    }
};