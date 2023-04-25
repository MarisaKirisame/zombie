#pragma once

#include <variant>

#include "basic.hpp"
#include "bag.hpp"
#include "fake_kinetic.hpp"
#include "kinetic.hpp"


enum class KineticHeapImpl {
  Bag = 0,
  Heap,
  Hanger
};


template<KineticHeapImpl impl, typename T, typename NotifyIndexChanged>
using KineticHeap = std::variant_alternative_t<
  (size_t)impl,
  std::variant<
    HeapImpls::Bag<T, NotifyIndexChanged>,
    HeapImpls::KineticMinHeap<T, false, NotifyIndexChanged>,
    HeapImpls::KineticMinHeap<T, true , NotifyIndexChanged>
  >
>;
