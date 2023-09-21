#pragma once

#include "heap/aff_function.hpp"
#include "heap/kinetic.hpp"

AffFunction *aff_function_new(slope_t slope, shift_t x_shift)
{
    return new AffFunction(slope, x_shift);
}

aff_t aff_function_eval(const AffFunction *aff_function, shift_t x)
{
    return (*aff_function)(x);
}

aff_t *aff_function_lt_until(const AffFunction *aff_function, const AffFunction *rhs)
{
    auto res = aff_function->lt_until(*rhs);
    if (res)
    {
        aff_t *rres = new aff_t(*res);
        return rres;
    }
    else
    {
        return nullptr;
    }
}

aff_t *aff_function_le_until(const AffFunction *aff_function, const AffFunction *rhs)
{
    auto res = aff_function->le_until(*rhs);
    if (res)
    {
        aff_t *rres = new aff_t(*res);
        return rres;
    }
    else
    {
        return nullptr;
    }
}

struct EmptyNotifyHeapIndexChanged
{
    void operator()(const void *&n, const size_t &idx) {}
};

using KineticHanger = HeapImpls::KineticMinHeap<void *, true, EmptyNotifyHeapIndexChanged>;

KineticHanger *kinetic_hanger_new(int64_t time)
{
    return new KineticHanger(time);
}