#pragma once

#include "heap/aff_function.hpp"
#include "heap/kinetic.hpp"

extern "C" {
  AffFunction *aff_function_new(slope_t slope, shift_t x_shift);
  aff_t aff_function_eval(const AffFunction *aff_function, shift_t x);
  aff_t* aff_function_lt_until(const AffFunction* aff_function, const AffFunction* rhs);
  aff_t* aff_function_le_until(const AffFunction* aff_function, const AffFunction* rhs);
  void aff_function_delete(AffFunction* aff_function);

  struct EmptyNotifyHeapIndexChanged;
  using KineticHanger = HeapImpls::KineticMinHeap<void*, true, EmptyNotifyHeapIndexChanged>;

  KineticHanger* kinetic_hanger_new(int64_t time);
  aff_t kinetic_hanger_cur_min_value(const KineticHanger* hanger);
  size_t kinetic_hanger_size(const KineticHanger* hanger);
  bool kinetic_hanger_empty(const KineticHanger* hanger);
  void kinetic_hanger_push(KineticHanger* hanger, void* t, const AffFunction* aff);
  void* kinetic_hanger_peek(KineticHanger* hanger);
  void* kinetic_hanger_index(KineticHanger* hanger, size_t i);
  void* kinetic_hanger_pop(KineticHanger* hanger);
  void* kinetic_hanger_remove(KineticHanger* hanger, size_t i);
  int64_t kinetic_hanger_time(const KineticHanger* hanger);
  void kinetic_hanger_advance_to(KineticHanger* hanger, int64_t new_time);
  void kinetic_hanger_delete(KineticHanger* hanger);

  struct MinNode {
    void* elm;
    double score;
  };
  struct CompareMinNode;
  struct NHICMinNode;
  struct NHERMinNode;
  using Heap = MinNormalHeap<MinNode, CompareMinNode, NHICMinNode, NHERMinNode>;

  Heap* heap_new();
  void heap_delete(Heap* heap);
  bool heap_empty(const Heap* heap);
  size_t heap_size(const Heap* heap);
  void heap_push(Heap* hanger, void* t, double score);
  double heap_peek_score(const Heap* heap);
  void* heap_peek(const Heap* heap);
  void* heap_pop(Heap* heap);
}
