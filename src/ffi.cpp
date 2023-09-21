#include "zombie/ffi.hpp"

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
    void operator()(void *n, size_t idx) {}
};

KineticHanger *kinetic_hanger_new(int64_t time)
{
    return new KineticHanger(time);
}

size_t kinetic_hanger_size(const KineticHanger *hanger)
{
    return hanger->size();
}

bool kinetic_hanger_empty(const KineticHanger *hanger)
{
    return hanger->empty();
}

void kinetic_hanger_insert(KineticHanger *hanger, void *t, const AffFunction *aff)
{
    hanger->insert(t, *aff);
}

void *kinetic_hanger_peek(KineticHanger *hanger)
{
    return hanger->peek();
}

void *kinetic_hanger_index(KineticHanger *hanger, size_t i)
{
    return (*hanger)[i];
}

void *kinetic_hanger_pop(KineticHanger *hanger)
{
    return hanger->pop();
}

void *kinetic_hanger_remove(KineticHanger *hanger, size_t i)
{
    return hanger->remove(i);
}

int64_t kinetic_hanger_time(const KineticHanger *hanger)
{
    return hanger->time();
}

void kinetic_hanger_advance_to(KineticHanger *hanger, int64_t new_time)
{
    return hanger->advance_to(new_time);
}
