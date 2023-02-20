#ifndef _CROW_FRACTAL_SERVER_ANALYZED_POLYNOMIAL_
#define _CROW_FRACTAL_SERVER_ANALYZED_POLYNOMIAL_

#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <limits>

#include "complex.h"
#include "complex_array.h"
#include "polynomial.h"

template <typename T>
Polynomial<T> Differentiate(const Polynomial<T>& p) {
  std::vector<Complex<T>> result;
  for (size_t i = 1; i < p.coefficients.size(); ++i) {
    result.push_back(p.coefficients[i] * Complex<T>(i, 0));
  }
  return Polynomial<T>(result);
}

template <typename T>
T ConservativeConvergenceRadius(const std::vector<Complex<T>>& zeros) {
  T min_distance = std::numeric_limits<T>::infinity();
  for (size_t i = 0; i < zeros.size(); i++) {
    for (size_t j = i + 1; j < zeros.size(); j++) {
      min_distance = std::min(min_distance, (zeros[i] - zeros[j]).magnitude());
    }
  }
  return min_distance / 20.0;
}

template <typename T>
class AnalyzedPolynomial {
 public:
  AnalyzedPolynomial<T>(const std::vector<Complex<T>>& zeros)
    : zeros(zeros),
      polynomial(Polynomial<T>::FromZeros(zeros)),
      derivative(Differentiate(polynomial)),
      convergence_radius(ConservativeConvergenceRadius(zeros)),
      sqr_convergence_radius(convergence_radius * convergence_radius) {
    assert(!zeros.empty());
  }

  template <typename ComplexValue>
  ComplexValue operator()(const ComplexValue& z) const {
    ComplexValue result = z - zeros[0];
    const size_t N = zeros.size();
    for (size_t i = 1; i < N; ++i) {
      result *= (z - zeros[i]);
    }
    return result;
  }

  template <typename ComplexValue>
  bool ConvergedToZero(const ComplexValue& z) const {
    for (const Complex<T>& zero : zeros) {
      if (z.CloseTo(zero, convergence_radius, sqr_convergence_radius)) {
	return true;
      }
    }
    return false;
  }

  template <typename ComplexValue>
  std::optional<size_t> GetZeroIndexIfConverged(const ComplexValue& z) const {
    size_t i = 0;
    for (const Complex<T>& zero : zeros) {
      if (z.CloseTo(zero, convergence_radius, sqr_convergence_radius)) {
	return i;
      }
      ++i;
    }
    return std::nullopt;
  }

  std::string ToString() const {
    std::ostringstream ss;
    ss << *this;
    return ss.str();
  }

  friend std::ostream& operator<<(std::ostream& os, const AnalyzedPolynomial<T>& a) {
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

  std::vector<Complex<T>> zeros;
  Polynomial<T> polynomial;
  Polynomial<T> derivative;
  T convergence_radius;
  T sqr_convergence_radius;
};

using AnalyzedPolynomialD = AnalyzedPolynomial<double>;
using AnalyzedPolynomialF = AnalyzedPolynomial<float>;

template <typename T, typename ComplexValue>
ComplexValue Newton(const AnalyzedPolynomial<T>& p, ComplexValue guess, size_t iterations, size_t* actual_iters = nullptr) {
  size_t i;
  for (i = 0; i < iterations; ++i) {
    if (p.ConvergedToZero(guess)) break;
    guess -= p(guess) / p.derivative(guess);
  }
  if (actual_iters != nullptr) {
    *actual_iters = i;
  }
  return guess;
}

template <typename T, typename ComplexValue>
void NewtonIter(const AnalyzedPolynomial<T>& p, ComplexValue* guess) {
  *guess -= p(*guess) / p.derivative(*guess);
}

template <typename T>
size_t ClosestZero(const Complex<T>& z, const std::vector<Complex<T>>& zeros) {
  size_t closest = 0;
  T closest_sqr_mag = (z - zeros[0]).sqr_magnitude();
  const size_t N = zeros.size();
  for (size_t i = 1; i < N; ++i) {
    T sqr_mag = (z - zeros[i]).sqr_magnitude();
    if (sqr_mag < closest_sqr_mag) {
      closest_sqr_mag = sqr_mag;
      closest = i;
    }
  }
  return closest;
}

#endif // _CROW_FRACTAL_SERVER_ANALYZED_POLYNOMIAL_
