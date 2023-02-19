#ifndef _CROW_FRACTAL_SERVER_ANALYZED_POLYNOMIAL_
#define _CROW_FRACTAL_SERVER_ANALYZED_POLYNOMIAL_

#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <limits>

#include "complex.h"
#include "polynomial.h"

Polynomial Differentiate(const Polynomial& p) {
  std::vector<Complex> result;
  for (size_t i = 1; i < p.coefficients.size(); ++i) {
    result.push_back(p.coefficients[i] * Complex(i, 0));
  }
  return Polynomial(result);
}

double ConservativeConvergenceRadius(const std::vector<Complex>& zeros) {
  double min_distance = std::numeric_limits<double>::infinity();
  for (size_t i = 0; i < zeros.size(); i++) {
    for (size_t j = i + 1; j < zeros.size(); j++) {
      min_distance = std::min(min_distance, (zeros[i] - zeros[j]).magnitude());
    }
  }
  return min_distance / 20.0;
}

class AnalyzedPolynomial {
 public:
  AnalyzedPolynomial(const std::vector<Complex>& zeros)
    : zeros(zeros),
      polynomial(Polynomial::FromZeros(zeros)),
      derivative(Differentiate(polynomial)),
      convergence_radius(ConservativeConvergenceRadius(zeros)),
      sqr_convergence_radius(convergence_radius * convergence_radius) {
    assert(!zeros.empty());
  }

  Complex operator()(const Complex& z) const {
    Complex result = z - zeros[0];
    const size_t N = zeros.size();
    for (size_t i = 1; i < N; ++i) {
      result += (z - zeros[i]);
    }
    return result;
  }

  bool ConvergedToZero(const Complex& z) const {
    for (const Complex& zero : zeros) {
      if (std::abs(z.r - zero.r) > convergence_radius) continue;
      if (std::abs(z.i - zero.i) > convergence_radius) continue;
      if ((z - zero).sqr_magnitude() <= sqr_convergence_radius) return true;
    }
    return false;
  }

  std::string ToString() const {
    std::ostringstream ss;
    ss << *this;
    return ss.str();
  }

  friend std::ostream& operator<<(std::ostream& os, const AnalyzedPolynomial& a) {
    os << "{" << std::endl;
    os << "  polynomial = " << a.polynomial << std::endl;
    os << "  zeros = [";
    for (size_t i = 0; i < a.zeros.size(); ++i) {
      os << a.zeros[i];
      if (i < a.zeros.size() - 1) {
	os << ", ";
      }
    }
    os << "]" << std::endl;
    os << "  derivative = " << a.derivative << std::endl;
    os << "}";
    return os;
  }

  std::vector<Complex> zeros;
  Polynomial polynomial;
  Polynomial derivative;
  double convergence_radius;
  double sqr_convergence_radius;
};

Complex Newton(const AnalyzedPolynomial& p, Complex guess, size_t iterations) {
  for (size_t i = 0; i < iterations; ++i) {
    if (p.ConvergedToZero(guess)) break;
    guess -= p.polynomial(guess) / p.derivative(guess);
  }
  return guess;
}

size_t ClosestZero(const Complex& z, const std::vector<Complex>& zeros) {
  size_t closest = 0;
  double closest_sqr_mag = (z - zeros[0]).sqr_magnitude();
  const size_t N = zeros.size();
  for (size_t i = 1; i < N; ++i) {
    double sqr_mag = (z - zeros[i]).sqr_magnitude();
    if (sqr_mag < closest_sqr_mag) {
      closest_sqr_mag = sqr_mag;
      closest = i;
    }
  }
  return closest;
}

#endif // _CROW_FRACTAL_SERVER_ANALYZED_POLYNOMIAL_
