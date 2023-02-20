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
    for (size_t i = 0; i < N; ++i) { // NOT :(
      values[i] = element;
    }
  }

  ComplexArray<T, N>& operator+=(const ComplexArray<T, N>& other) {
    for (size_t i = 0; i < N; ++i) { // OPTIMIZED
      values[i] += other.values[i];
    }
    return *this;
  }

  ComplexArray<T, N>& operator*=(const ComplexArray<T, N>& other) {
    // std::cout << "USED *=" << std::endl;
    for (size_t i = 0; i < N; ++i) {
      values[i] *= other.values[i];
    }
    return *this;
  }

  ComplexArray<T, N>& operator*=(const Complex<T>& element) {
    for (size_t i = 0; i < N; ++i) { // OPTIMIZED
      values[i] *= element;
    }
    return *this;
  }

  ComplexArray<T, N>& operator=(const Complex<T>& element) {
    for (size_t i = 0; i < N; ++i) {
      values[i] = element;
    }
    return *this;
  }

  std::array<Complex<T>, N> values;
};

template <typename T, size_t N>
ComplexArray<T, N>& operator*(const ComplexArray<T, N>& a, const ComplexArray<T, N>& b) {
  ComplexArray<T, N> result;
  for (size_t i = 0; i < N; ++i) {
    result.values[i] = a.values[i] * b.values[i];
  }
  return result;
}

template <typename T, size_t N>
ComplexArray<T, N> operator*(const ComplexArray<T, N>& a, const Complex<T>& b) {
  ComplexArray<T, N> result = a;
  result *= b;
  return result;
}

template <typename T, size_t N>
class ComplexArray2 {
 public:
  ComplexArray2<T, N>() {}
  ComplexArray2<T, N>(const Complex<T>& element) {
    for (size_t i = 0; i < N; ++i) { // OPTIMIZED
      rs[i] = element.r;
      is[i] = element.i;
    }
  }

  // Only returns true when *all* values are close to the given target.
  bool CloseTo(const Complex<T>& target,
	       T convergence_radius,
	       T sqr_convergence_radius) const {
    for (size_t i = 0; i < N; i++) {
      if (std::abs(rs[i] - target.r) > convergence_radius) return false;
      if (std::abs(is[i] - target.i) > convergence_radius) return false;
    }
    for (size_t i = 0; i < N; i++) {
      if ((get(i) - target).sqr_magnitude() > sqr_convergence_radius) {
	return false;
      }
    }
    return true;
  }

  ComplexArray2<T, N>& operator+=(const ComplexArray2<T, N>& other) {
    for (size_t i = 0; i < N; ++i) { // OPTIMIZED
      rs[i] += other.rs[i];
      is[i] += other.is[i];
    }
    return *this;
  }

  ComplexArray2<T, N>& operator-=(const ComplexArray2<T, N>& other) {
    for (size_t i = 0; i < N; ++i) {
      rs[i] -= other.rs[i];
      is[i] -= other.is[i];
    }
    return *this;
  }

  ComplexArray2<T, N>& operator*=(const ComplexArray2<T, N>& other) {
    // std::cout << "USED *= (complex 2)" << std::endl;
    for (size_t i = 0; i < N; ++i) {
      const T new_r = rs[i] * other.rs[i] - is[i] * other.is[i];
      const T new_i = rs[i] * other.is[i] + is[i] * other.rs[i];
      rs[i] = new_r;
      is[i] = new_i;
    }
    return *this;
  }

  Complex<T> get(size_t i) const {
    return Complex<T>(rs[i], is[i]);
  }

  std::array<T, N> rs;
  std::array<T, N> is;
};

template <typename T, size_t N>
ComplexArray2<T, N> operator*(const ComplexArray2<T, N>& a, const Complex<T>& element) {
  ComplexArray2<T, N> result;
  for (size_t i = 0; i < N; ++i) { // OPTIMIZED
    result.rs[i] = a.rs[i] * element.r - a.is[i] * element.i;
    result.is[i] = a.rs[i] * element.i + a.is[i] * element.r;
  }
  return result;
}

template <typename T, size_t N>
ComplexArray2<T, N> operator-(const ComplexArray2<T, N>& a, const Complex<T>& element) {
  ComplexArray2<T, N> result;
  for (size_t i = 0; i < N; ++i) { // OPTIMIZED
    result.rs[i] = a.rs[i] - element.r;
    result.is[i] = a.is[i] - element.i;
  }
  return result;
}

template <typename T, size_t N>
ComplexArray2<T, N> operator/(const ComplexArray2<T, N>& a, const ComplexArray2<T, N>& b) {
  ComplexArray2<T, N> result;
  for (size_t i = 0; i < N; ++i) {
    const T denom = b.rs[i] * b.rs[i] + b.is[i] * b.is[i];
    result.rs[i] = (a.rs[i] * b.rs[i] + a.is[i] * b.is[i]) / denom;
    result.is[i] = (a.is[i] * b.rs[i] - a.rs[i] * b.is[i]) / denom;
  }
  return result;
}

#endif // _CROW_FRACTAL_SERVER_COMPLEX_
