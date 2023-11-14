#include "zombie/ffi.hpp"

AffFunction *aff_function_new(slope_t slope, shift_t x_shift) {
  return new AffFunction(slope, x_shift);
}

aff_t aff_function_eval(const AffFunction *aff_function, shift_t x) {
  return (*aff_function)(x);
}

aff_t *aff_function_lt_until(const AffFunction *aff_function, const AffFunction *rhs) {
  auto res = aff_function->lt_until(*rhs);
  if (res) {
    aff_t *rres = new aff_t(*res);
    return rres;
  }
  else {
    return nullptr;
  }
}

aff_t *aff_function_le_until(const AffFunction *aff_function, const AffFunction *rhs) {
  auto res = aff_function->le_until(*rhs);
  if (res) {
    aff_t *rres = new aff_t(*res);
    return rres;
  }
  else {
    return nullptr;
  }
}

void aff_function_delete(AffFunction *aff_function) {
  delete aff_function;
}


struct EmptyNotifyHeapIndexChanged {
  void operator()(void *n, size_t idx) {}
};

KineticHanger *kinetic_hanger_new(int64_t time) {
  return new KineticHanger(time);
}

aff_t kinetic_hanger_cur_min_value(const KineticHanger* hanger) {
  return hanger->cur_min_value();
}

size_t kinetic_hanger_size(const KineticHanger *hanger) {
  return hanger->size();
}

bool kinetic_hanger_empty(const KineticHanger *hanger) {
  return hanger->empty();
}

void kinetic_hanger_push(KineticHanger *hanger, void *t, const AffFunction *aff) {
  hanger->push(t, *aff);
}

void *kinetic_hanger_peek(KineticHanger *hanger) {
  return hanger->peek();
}

void *kinetic_hanger_index(KineticHanger *hanger, size_t i) {
  return (*hanger)[i];
}

void *kinetic_hanger_pop(KineticHanger *hanger) {
  return hanger->pop();
}

void *kinetic_hanger_remove(KineticHanger *hanger, size_t i) {
  return hanger->remove(i);
}

int64_t kinetic_hanger_time(const KineticHanger *hanger) {
  return hanger->time();
}

void kinetic_hanger_advance_to(KineticHanger *hanger, int64_t new_time) {
  return hanger->advance_to(new_time);
}

void kinetic_hanger_delete(KineticHanger *hanger) {
  delete hanger;
}

struct CompareMinNode {
  bool operator()(const MinNode& l, const MinNode& r) {
    return l.score < r.score;
  }
};
struct NHICMinNode {
  void operator()(const MinNode&, size_t) {}
};
struct NHERMinNode {
  void operator()(const MinNode&) {}
};

Heap* heap_new() {
  return new Heap();
}
void heap_delete(Heap* heap) {
  delete heap;
}
size_t heap_size(const Heap* heap) {
  return heap->size();
}
void heap_push(Heap* heap, void* t, double score) {
  heap->push({t, score});
}
double heap_peek_score(const Heap* heap) {
  return heap->peek().score;
}
void* heap_peek(const Heap* heap) {
  return heap->peek().elm;
}
void* heap_pop(Heap* heap) {
  return heap->pop().elm;
}
