#ifndef _CROW_FRACTAL_SERVER_COMPLEX_
#define _CROW_FRACTAL_SERVER_COMPLEX_

#include <string>
#include <iostream>
#include <sstream>
#include <math.h>

template <typename T>
class Complex;

template <typename T>
Complex<T> operator*(const Complex<T>& a, const Complex<T>& b);

template <typename T>
Complex<T> operator/(const Complex<T>& a, const Complex<T>& b);

using ComplexD = Complex<double>;
using ComplexF = Complex<float>;

template <typename T>
class Complex {
 public:
  Complex<T>() : Complex(0.0, 0.0) {}
  Complex<T>(T r, T i) : r(r), i(i) {}

  T sqr_magnitude() const {
    return r * r + i * i;
  }

  T magnitude() const {
    return sqrt(sqr_magnitude());
  }

  bool CloseTo(const Complex<T>& target,
	       T convergence_radius,
	       T sqr_convergence_radius) const {
    if (std::abs(r - target.r) > convergence_radius) return false;
    if (std::abs(i - target.i) > convergence_radius) return false;
    return (*this - target).sqr_magnitude() <= sqr_convergence_radius;
  }

  std::string ToString() const {
    std::ostringstream ss;
    ss << *this;
    return ss.str();
  }

  friend std::ostream& operator<<(std::ostream& os, const Complex<T>& c) {
    os << c.r << "+" << c.i << "i";
    return os;
  }

  Complex<T>& operator+=(const Complex<T>& other) {
    r += other.r;
    i += other.i;
    return *this;
  }

  Complex<T>& operator-=(const Complex<T>& other) {
    r -= other.r;
    i -= other.i;
    return *this;
  }

  Complex<T>& operator*=(const Complex<T>& other) {
    *this = *this * other;
    return *this;
  }

  Complex<T>& operator/=(const Complex<T>& other) {
    *this = *this / other;
    return *this;
  }

  T r;
  T i;
};

template <typename T>
bool operator==(const Complex<T>& a, const Complex<T>& b) {
  return a.r == b.r && a.i == b.i;
}

template <typename T>
bool operator!=(const Complex<T>& a, const Complex<T>& b) {
  return !(a == b);
}

template <typename T>
Complex<T> operator+(const Complex<T>& a, const Complex<T>& b) {
  return Complex<T>(a.r + b.r, a.i + b.i);
}

template <typename T>
Complex<T> operator-(const Complex<T>& a, const Complex<T>& b) {
  return Complex<T>(a.r - b.r, a.i - b.i);
}

template <typename T>
Complex<T> operator*(const Complex<T>& a, const Complex<T>& b) {
  return Complex<T>(a.r * b.r - a.i * b.i,
		    a.r * b.i + a.i * b.r);
}

template <typename T>
Complex<T> operator/(const Complex<T>& a, const Complex<T>& b) {
  const T denom = b.r * b.r + b.i * b.i;
  return Complex<T>((a.r * b.r + a.i * b.i) / denom,
		    (a.i * b.r - a.r * b.i) / denom);
}

template <typename T>
Complex<T> operator-(const Complex<T>& a) {
  return Complex<T>(-a.r, -a.i);
}

#endif // _CROW_FRACTAL_SERVER_COMPLEX_
