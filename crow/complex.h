#ifndef _CROW_FRACTAL_SERVER_COMPLEX_
#define _CROW_FRACTAL_SERVER_COMPLEX_

#include <string>
#include <iostream>
#include <sstream>

class Complex;
Complex operator*(const Complex& a, const Complex& b);
Complex operator/(const Complex& a, const Complex& b);

class Complex {
 public:
  Complex() : Complex(0.0, 0.0) {}
  Complex(double r, double i) : r(r), i(i) {}

  double sqr_magnitude() const {
    return r * r + i * i;
  }

  std::string ToString() const {
    std::ostringstream ss;
    ss << *this;
    return ss.str();
  }

  friend std::ostream& operator<<(std::ostream& os, const Complex& c) {
    os << c.r << "+" << c.i << "i";
    return os;
  }

  Complex& operator+=(const Complex& other) {
    r += other.r;
    i += other.i;
    return *this;
  }

  Complex& operator-=(const Complex& other) {
    r -= other.r;
    i -= other.i;
    return *this;
  }

  Complex& operator*=(const Complex& other) {
    *this = *this * other;
    return *this;
  }

  Complex& operator/=(const Complex& other) {
    *this = *this / other;
    return *this;
  }

  double r;
  double i;
};

Complex operator+(const Complex& a, const Complex& b) {
  return Complex(a.r + b.r, a.i + b.i);
}

Complex operator-(const Complex& a, const Complex& b) {
  return Complex(a.r - b.r, a.i - b.i);
}

Complex operator*(const Complex& a, const Complex& b) {
  return Complex(a.r * b.r - a.i * b.i,
		 a.r * b.i + a.i * b.r);
}

Complex operator/(const Complex& a, const Complex& b) {
  const double denom = b.r * b.r + b.i * b.i;
  return Complex((a.r * b.r + a.i * b.i) / denom,
		 (a.i * b.r - a.r * b.i) / denom);
}

Complex operator-(const Complex& a) {
  return Complex(-a.r, -a.i);
}

#endif // _CROW_FRACTAL_SERVER_COMPLEX_
