#ifndef _CROW_FRACTAL_SERVER_POLYNOMIAL_
#define _CROW_FRACTAL_SERVER_POLYNOMIAL_

#include <vector>
#include <string>
#include <iostream>
#include <sstream>

#include "complex.h"

class Polynomial;
Polynomial operator*(const Polynomial& a, const Polynomial& b);

class Polynomial {
 public:
  explicit Polynomial(const std::vector<Complex>& coefficients)
   : coefficients(coefficients) {
    assert(!coefficients.empty());
  }

  static Polynomial FromZeros(const std::vector<Complex>& zeros) {
    Polynomial result({Complex(1, 0)}); // p(z) = 1
    for (const Complex& zero : zeros) {
      result *= Polynomial({-zero, Complex(1, 0)});
    }
    return result;
  }

  Complex operator()(const Complex& z) const {
    Complex result = coefficients[0];
    Complex z_pow = z;
    size_t i = 1;
    const size_t N = coefficients.size() - 1;
    for (; i < N; ++i) {
        result += coefficients[i] * z_pow;
        z_pow *= z;
    }
    result += coefficients[i] * z_pow;
    return result;
  }

  std::string ToString() const {
    std::ostringstream ss;
    ss << *this;
    return ss.str();
  }

  friend std::ostream& operator<<(std::ostream& os, const Polynomial& p) {
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

  Polynomial& operator*=(const Polynomial& other) {
    *this = *this * other;
    return *this;
  }

  std::vector<Complex> coefficients;
};

Polynomial operator*(const Polynomial& a, const Polynomial& b) {
  const size_t A = a.coefficients.size();
  const size_t B = b.coefficients.size();
  if (A == 0 || B == 0) {
    return Polynomial({});
  }
  std::vector<Complex> result(A + B - 1);
  for (size_t i = 0; i < A; ++i) {
    for (size_t j = 0; j < B; ++j) {
      result[i + j] +=  a.coefficients[i] * b.coefficients[j];
    }
  }
  return Polynomial(result);
}

#endif // _CROW_FRACTAL_SERVER_POLYNOMIAL_
