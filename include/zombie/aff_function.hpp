#pragma once

#include "common.hpp"

#include <cstdint>
#include <optional>
#include <iostream>

template<typename T>
T div_ceiling(T x, T y) {
  if (y < 0) {
    x = -x;
    y = -y;
  }
  if (x > 0) {
    return ((x - 1) / y) + 1;
  } else {
    return x / y;
  }
}

using slope_t = int128_t;
using shift_t = int64_t;
using aff_t = int128_t;
// An affine function.
// Note that it is in i64_t,
// because floating point scare me.
struct AffFunction {
  // Typically we have y_shift instead of x_shift.
  // We choose x_shift as this make updating latency much easier.
  // This should not effect the implementation of kinetc data structure in anyway.
  slope_t slope;
  shift_t x_shift;

  friend std::ostream& operator<<(std::ostream& os, const AffFunction& f);

  AffFunction(slope_t slope, shift_t x_shift) : slope(slope), x_shift(x_shift) { }

  aff_t operator()(shift_t x) const {
    auto ret = slope * (x + x_shift);
    // technically speaking I cant check overflow like this as overflow is UB.
    assert((slope == 0) || (ret / slope == (x + x_shift)));
    return ret;
  }

  std::optional<aff_t> lt_until(const AffFunction& rhs) const {
    auto postcondition_check = [&](aff_t val) {
      assert((*this)(val - 1) < rhs(val - 1));
      assert(!((*this)(val) < rhs(val)));
      return std::optional<aff_t>(val);
    };
    shift_t x_delta = rhs.x_shift - x_shift;
    aff_t y_delta = rhs.slope * x_delta;
    // note that x_delta is based on rhs, but slope_delta is based on *this.
    slope_t slope_delta = slope - rhs.slope;
    if (slope_delta <= 0) {
      return std::optional<aff_t>();
    } else {
      assert(slope_delta > 0);
      return postcondition_check(-x_shift + div_ceiling(y_delta, slope_delta));
    }
  }

  // return Some(x) if,
  //   forall x' < x,
  //     (*this)(x') <= rhs(x') /\
  //     !((*this)(x) <= rhs(x)).
  //   note that due to property of Affine Function,
  //     forall x' > x,
  //     !((*this)(x') <= rhs(x'))
  // return None if such x does not exist.
  std::optional<aff_t> le_until(const AffFunction& rhs) const {
    auto postcondition_check = [&](aff_t val) {
      assert((*this)(val - 1) <= rhs(val - 1));
      assert(!((*this)(val) <= rhs(val)));
      return std::optional<aff_t>(val);
    };
    // let's assume the current time is -x_shift,
    //   so eval(cur_time) == 0, and rhs.eval(cur_time) == rhs.slope * x_delta.
    // we then make lhs catch up(or fall back) to rhs
    shift_t x_delta = rhs.x_shift - x_shift;
    aff_t y_delta = rhs.slope * x_delta;
    // note that x_delta is based on rhs, but slope_delta is based on *this.
    slope_t slope_delta = slope - rhs.slope;
    if (slope_delta <= 0) {
      return std::optional<aff_t>();
    } else {
      assert(slope_delta > 0);
      return postcondition_check(-x_shift + div_ceiling(y_delta + 1, slope_delta));
    }
  }

  std::optional<aff_t> gt_until(const AffFunction& rhs) const {
    return rhs.lt_until(*this);
  }

  std::optional<aff_t> ge_until(const AffFunction& rhs) const {
    return rhs.le_until(*this);
  }

};

inline std::ostream &operator<<(std::ostream &out, int128_t val) {
  assert(val <= std::numeric_limits<int64_t>::max());
  return out << static_cast<uint64_t>(val);
}

inline std::ostream& operator<<(std::ostream& os, const AffFunction& f) {
  return os << "(" << f.slope << "(x+" << f.x_shift << "))";
}
