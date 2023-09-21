#pragma once

#include "heap/aff_function.hpp"
#include "heap/kinetic.hpp"

extern "C"
{
    AffFunction *aff_function_new(slope_t slope, shift_t x_shift);

    aff_t aff_function_eval(const AffFunction *aff_function, shift_t x);

    aff_t *aff_function_lt_until(const AffFunction *aff_function, const AffFunction *rhs);

    aff_t *aff_function_le_until(const AffFunction *aff_function, const AffFunction *rhs);

    struct EmptyNotifyHeapIndexChanged;

    using KineticHanger = HeapImpls::KineticMinHeap<void *, true, EmptyNotifyHeapIndexChanged>;

    KineticHanger *kinetic_hanger_new(int64_t time);

    size_t kinetic_hanger_size(const KineticHanger *hanger);

    bool kinetic_hanger_empty(const KineticHanger *hanger);

    void kinetic_hanger_insert(KineticHanger *hanger, void *t, const AffFunction *aff);

    void *kinetic_hanger_peek(KineticHanger *hanger);

    void *kinetic_hanger_index(KineticHanger *hanger, size_t i);

    void *kinetic_hanger_pop(KineticHanger *hanger);

    void *kinetic_hanger_remove(KineticHanger *hanger, size_t i);

    int64_t kinetic_hanger_time(const KineticHanger *hanger);

    void kinetic_hanger_advance_to(KineticHanger *hanger, int64_t new_time);
}