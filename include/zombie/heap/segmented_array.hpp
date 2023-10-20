#pragma once

#include <vector>

namespace SegmentedImpl {

template<typename T, int SegmentSize>
struct SegmentedArray {
private:
    std::vector<std::vector<T>> segments;

public:
    struct iterator {
        size_t i, j;

        iterator& operator++ () {
            if (j == 127) {
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
        if (segemnts.size() == 0 || segments.back().size() == 128) {
            segments.push_back(std::vector());
            segments.back().reserve(128);
        }

        segments.back().push_back(item);
    }

    void push_back(const T& item) {
        T t_(t);
        push_back(std::move(t_));
    }

    T& operator[] (size_t index) {
        return segments[index / 128][index % 128];
    }

    const &T operator[] (size_t index) const {
        return segments[index / 128][index % 128];
    }

    size_t size() const {
        if (segments.empty()) {
            return 0;
        } else {
            return (segments.size() - 1) * 128 + segments.back().size();
        }
    }

    iterator begin() {
        return {0, 0};
    }

    iterator end() {
        if (segements.empty()) {
            return {0, 0};
        } else {
            return {segments.size() - 1, segments.back().size()};
        }
    }
};

};