#ifndef _CROW_FRACTAL_SERVER_POLYNOMIAL_
#define _CROW_FRACTAL_SERVER_POLYNOMIAL_

#include <vector>
#include <string>
#include <iostream>
#include <sstream>

#include "complex.h"

template <typename T>
class Polynomial;

template <typename T>
Polynomial<T> operator*(const Polynomial<T>& a, const Polynomial<T>& b);

using PolynomialD = Polynomial<double>;
using PolynomialF = Polynomial<float>;

template <typename T>
class Polynomial {
 public:
  explicit Polynomial<T>(const std::vector<Complex<T>>& coefficients)
   : coefficients(coefficients) {
    assert(!coefficients.empty());
  }

  static Polynomial<T> FromZeros(const std::vector<Complex<T>>& zeros) {
    Polynomial<T> result({Complex<T>(1, 0)}); // p(z) = 1
    for (const Complex<T>& zero : zeros) {
      result *= Polynomial<T>({-zero, Complex<T>(1, 0)});
    }
    return result;
  }

  template<typename ComplexValue>
  ComplexValue operator()(const ComplexValue& z) const {
    ComplexValue result = coefficients[0];
    ComplexValue z_pow = z;
    size_t i = 1;
    const size_t N = coefficients.size() - 1;
    for (; i < N; ++i) {
        result += z_pow * coefficients[i];
        z_pow *= z;
    }
    result += z_pow * coefficients[i];
    return result;
  }

  std::string ToString() const {
    std::ostringstream ss;
    ss << *this;
    return ss.str();
  }

  friend std::ostream& operator<<(std::ostream& os, const Polynomial<T>& p) {
    for (size_t i = 0; i < p.coefficients.size(); ++i) {
      os << "(" << p.coefficients[i] << ")";
      if (i > 0) {
	os << "z^" << i;
      }
      if (i < p.coefficients.size() - 1) {
	os << " + ";
      }
    }
    return os;
  }

  Polynomial<T>& operator*=(const Polynomial<T>& other) {
    *this = *this * other;
    return *this;
  }

  std::vector<Complex<T>> coefficients;
};

template <typename T>
Polynomial<T> operator*(const Polynomial<T>& a, const Polynomial<T>& b) {
  const size_t A = a.coefficients.size();
  const size_t B = b.coefficients.size();
  if (A == 0 || B == 0) {
    return Polynomial<T>({});
  }
  std::vector<Complex<T>> result(A + B - 1);
  for (size_t i = 0; i < A; ++i) {
    for (size_t j = 0; j < B; ++j) {
      result[i + j] +=  a.coefficients[i] * b.coefficients[j];
    }
  }
  return Polynomial<T>(result);
}

#endif // _CROW_FRACTAL_SERVER_POLYNOMIAL_
