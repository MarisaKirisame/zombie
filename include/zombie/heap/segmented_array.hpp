#pragma once

#include <vector>

namespace SegmentedImpl {

const size_t SegmentSize = 128;

template<typename T>
struct SegmentedArray {
private:
    std::vector<std::vector<T>> segments;

public:
    struct iterator {
        size_t i, j;

        iterator& operator++ () {
            if (j + 1 == SegmentSize) {
                i++; j = 0;
            } else {
                j++;
            }

            return *this;
        }

        T& operator*() {
            return segments[i][j];
        }
    };

    void push_back(T&& item) {
        if (segments.size() == 0 || segments.back().size() == SegmentSize) {
            segments.push_back(std::vector());
            segments.back().reserve(SegmentSize);
        }

        segments.back().push_back(item);
    }

    void push_back(const T& item) {
        T t_(t);
        push_back(std::move(t_));
    }

    T& operator[] (size_t index) {
        return segments[index / SegmentSize][index % SegmentSize];
    }

    const T operator[] (size_t index) const {
        return segments[index / SegmentSize][index % SegmentSize];
    }

    size_t size() const {
        if (segments.empty()) {
            return 0;
        } else {
            return (segments.size() - 1) * SegmentSize + segments.back().size();
        }
    }

    iterator begin() {
        return {0, 0};
    }

    iterator end() {
        if (segments.empty()) {
            return {0, 0};
        } else {
            return {segments.size() - 1, segments.back().size()};
        }
    }
};

};