#ifndef _CROW_FRACTAL_SERVER_COMPLEX_ARRAY_EIGEN_
#define _CROW_FRACTAL_SERVER_COMPLEX_ARRAY_EIGEN_

#include <string>
#include <array>
#include <iostream>
#include <sstream>
#include <math.h>

#include <Eigen/Dense>

#include "complex.h"

template <typename T, size_t N>
class ComplexArray {
 public:
  ComplexArray<T, N>() {}
  ComplexArray<T, N>(const Complex<T>& element) {
    rs_ = element.r;
    is_ = element.i;
  }

  // Only returns true when *all* values are close to the given target.
  bool CloseTo(const Complex<T>& target,
	       T convergence_radius,
	       T sqr_convergence_radius) const {
    for (size_t i = 0; i < N; i++) {
      if (std::abs(rs_(i) - target.r) > convergence_radius) return false;
      if (std::abs(is_(i) - target.i) > convergence_radius) return false;
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
    rs_ += other.rs_;
    is_ += other.is_;
    return *this;
  }

  ComplexArray<T, N>& operator-=(const ComplexArray<T, N>& other) {
    rs_ -= other.rs_;
    is_ -= other.is_;
    return *this;
  }

  ComplexArray<T, N>& operator*=(const ComplexArray<T, N>& other) {
    Eigen::Array<T, N, 1> new_rs = rs_ * other.rs_ - is_ * other.is_;
    Eigen::Array<T, N, 1> new_is = rs_ * other.is_ + is_ * other.rs_;
    rs_ = new_rs;
    is_ = new_is;
    return *this;
  }

  // Accessors.

  Complex<T> get(size_t i) const {
    return Complex<T>(rs_(i), is_(i));
  }

  T& rs(size_t i) {
    return rs_(i);
  };
  const T& rs(size_t i) const {
    return rs_(i);
  };

  T& is(size_t i) {
    return is_(i);
  };
  const T& is(size_t i) const {
    return is_(i);
  };

  // Friend operators.

  friend ComplexArray<T, N> operator*(const ComplexArray<T, N>& a, const Complex<T>& element) {
    ComplexArray<T, N> result;
    result.rs_ = a.rs_ * element.r - a.is_ * element.i;
    result.is_ = a.rs_ * element.i + a.is_ * element.r;
    return result;
  }

  friend ComplexArray<T, N> operator-(const ComplexArray<T, N>& a, const Complex<T>& element) {
    ComplexArray<T, N> result;
    result.rs_ = a.rs_ - element.r;
    result.is_ = a.is_ - element.i;
    return result;
  }

  friend ComplexArray<T, N> operator/(const ComplexArray<T, N>& a, const ComplexArray<T, N>& b) {
    ComplexArray<T, N> result;
    Eigen::Array<T, N, 1> denoms = b.rs_ * b.rs_ + b.is_ * b.is_;
    result.rs_ = (a.rs_ * b.rs_ + a.is_ * b.is_) / denoms;
    result.is_ = (a.is_ * b.rs_ - a.rs_ * b.is_) / denoms;
    return result;
  }

 private:
  Eigen::Array<T, N, 1>  rs_;
  Eigen::Array<T, N, 1>  is_;
};

#endif // _CROW_FRACTAL_SERVER_COMPLEX_ARRAY_EIGEN_
