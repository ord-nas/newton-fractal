#ifndef _CROW_FRACTAL_SERVER_COMPLEX_ARRAY_
#define _CROW_FRACTAL_SERVER_COMPLEX_ARRAY_

#include <string>
#include <array>
#include <iostream>
#include <sstream>
#include <math.h>

#include "complex.h"

template <typename T, size_t N>
class ComplexArray {
 public:
  ComplexArray<T, N>() {}
  ComplexArray<T, N>(const Complex<T>& element) {
    for (size_t i = 0; i < N; ++i) {
      rs_[i] = element.r;
      is_[i] = element.i;
    }
  }

  // Only returns true when *all* values are close to the given target.
  bool CloseTo(const Complex<T>& target,
	       T convergence_radius,
	       T sqr_convergence_radius) const {
    for (size_t i = 0; i < N; i++) {
      if (std::abs(rs_[i] - target.r) > convergence_radius) return false;
      if (std::abs(is_[i] - target.i) > convergence_radius) return false;
    }
    for (size_t i = 0; i < N; i++) {
      if ((get(i) - target).sqr_magnitude() > sqr_convergence_radius) {
	return false;
      }
    }
    return true;
  }

  // Member operators.

  ComplexArray<T, N>& operator+=(const ComplexArray<T, N>& other) {
    for (size_t i = 0; i < N; ++i) {
      rs_[i] += other.rs_[i];
      is_[i] += other.is_[i];
    }
    return *this;
  }

  ComplexArray<T, N>& operator-=(const ComplexArray<T, N>& other) {
    for (size_t i = 0; i < N; ++i) {
      rs_[i] -= other.rs_[i];
      is_[i] -= other.is_[i];
    }
    return *this;
  }

  ComplexArray<T, N>& operator*=(const ComplexArray<T, N>& other) {
    for (size_t i = 0; i < N; ++i) {
      const T new_r = rs_[i] * other.rs_[i] - is_[i] * other.is_[i];
      const T new_i = rs_[i] * other.is_[i] + is_[i] * other.rs_[i];
      rs_[i] = new_r;
      is_[i] = new_i;
    }
    return *this;
  }

  // Accessors.

  Complex<T> get(size_t i) const {
    return Complex<T>(rs_[i], is_[i]);
  }

  T& rs(size_t i) {
    return rs_[i];
  };
  const T& rs(size_t i) const {
    return rs_[i];
  };

  T& is(size_t i) {
    return is_[i];
  };
  const T& is(size_t i) const {
    return is_[i];
  };

  // Friend operators.

  friend ComplexArray<T, N> operator*(const ComplexArray<T, N>& a, const Complex<T>& element) {
    ComplexArray<T, N> result;
    for (size_t i = 0; i < N; ++i) {
      result.rs_[i] = a.rs_[i] * element.r - a.is_[i] * element.i;
      result.is_[i] = a.rs_[i] * element.i + a.is_[i] * element.r;
    }
    return result;
  }

  friend ComplexArray<T, N> operator-(const ComplexArray<T, N>& a, const Complex<T>& element) {
    ComplexArray<T, N> result;
    for (size_t i = 0; i < N; ++i) {
      result.rs_[i] = a.rs_[i] - element.r;
      result.is_[i] = a.is_[i] - element.i;
    }
    return result;
  }

  friend ComplexArray<T, N> operator/(const ComplexArray<T, N>& a, const ComplexArray<T, N>& b) {
    ComplexArray<T, N> result;
    for (size_t i = 0; i < N; ++i) {
      const T denom = b.rs_[i] * b.rs_[i] + b.is_[i] * b.is_[i];
      result.rs_[i] = (a.rs_[i] * b.rs_[i] + a.is_[i] * b.is_[i]) / denom;
      result.is_[i] = (a.is_[i] * b.rs_[i] - a.rs_[i] * b.is_[i]) / denom;
    }
    return result;
  }

 private:
  std::array<T, N> rs_;
  std::array<T, N> is_;
};

#endif // _CROW_FRACTAL_SERVER_COMPLEX_
