#pragma once

#include <iostream>
#include <cassert>
#include <functional>
#include <memory>
#include <optional>

#include "common.hpp"

namespace TockTreeCaches {

template<typename Node>
struct NoneCache {
    std::optional<std::shared_ptr<Node>> get(const Tock& t) {
        return {};
    }

    void update(const Tock& t, const std::shared_ptr<Node>& p) {
    }
};

}; // end of namespace TockTreeCaches