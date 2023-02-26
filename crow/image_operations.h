#ifndef _CROW_FRACTAL_SERVER_IMAGE_OPERATIONS_
#define _CROW_FRACTAL_SERVER_IMAGE_OPERATIONS_

#include "rgb_image.h"
#include "image_regions.h"

struct IntRGBPixel {
  int r;
  int g;
  int b;

  static IntRGBPixel FromChar(const png::rgb_pixel& p) {
    return {p.red, p.green, p.blue};
  }

  png::rgb_pixel ToChar() const {
    return {static_cast<unsigned char>(r),
	    static_cast<unsigned char>(g),
	    static_cast<unsigned char>(b)};
  }

  IntRGBPixel operator+(const IntRGBPixel& other) const {
    return {r + other.r, g + other.g, b + other.b};
  }

  IntRGBPixel operator+(int offset) const {
    return {r + offset, g + offset, b + offset};
  }

  IntRGBPixel operator*(int scale) const {
    return {r * scale, g * scale, b * scale};
  }

  IntRGBPixel operator>>(int shift) const {
    return {r >> shift, g >> shift, b >> shift};
  }
};

// Can likely be optimized, but not clear if necessary.
void CopyImage(const RGBImage& from, RGBImage& to, const ImageOverlap& overlap) {
  size_t from_y = overlap.a_region.y_min;
  size_t to_y = overlap.b_region.y_min;
  for (; from_y < overlap.a_region.y_max; ++from_y, ++to_y) {
    size_t from_x = overlap.a_region.x_min;
    size_t to_x = overlap.b_region.x_min;
    for (; from_x < overlap.a_region.x_max; ++from_x, ++to_x) {
      to[to_y][to_x] = from[from_y][from_x];
    }
  }
}

void ResizeBilinear(const RGBImage& from, RGBImage& to, const ImageOverlap& overlap) {
  constexpr int kFactor = 2048;
  constexpr int kShift = 11;

  const ImageRect& from_rect = overlap.a_region;
  const ImageRect& to_rect = overlap.b_region;
  const int x_scale_i = static_cast<int>(1.0 * kFactor * from_rect.width() / to_rect.width() + 0.5);
  const int y_scale_i = static_cast<int>(1.0 * kFactor * from_rect.height() / to_rect.height() + 0.5);

  for (int y = 0; y < to_rect.height(); ++y) {
    const int to_y = y + to_rect.y_min;
    const int from_y_i = y * y_scale_i + from_rect.y_min * kFactor;
    const int from_y_0 = from_y_i >> kShift;
    const int from_y_frac = from_y_i - (from_y_0 << kShift);

    if (from_y_0 < 0 || from_y_0 + 1 >= from.get_height()) {
      continue;
    }

    for (int x = 0; x < to_rect.width(); ++x) {
      const int to_x = x + to_rect.x_min;
      const int from_x_i = x * x_scale_i + from_rect.x_min * kFactor;;
      const int from_x_0 = from_x_i >> kShift;
      const int from_x_frac = from_x_i - (from_x_0 << kShift);

      if (from_x_0 < 0 || from_x_0 + 1 >= from.get_width()) {
	continue;
      }

      const auto P00 = IntRGBPixel::FromChar(from[from_y_0][from_x_0]) * (kFactor - from_x_frac) * (kFactor - from_y_frac);
      const auto P01 = IntRGBPixel::FromChar(from[from_y_0][from_x_0 + 1]) * from_x_frac * (kFactor - from_y_frac);
      const auto P10 = IntRGBPixel::FromChar(from[from_y_0 + 1][from_x_0]) * (kFactor - from_x_frac) * from_y_frac;
      const auto P11 = IntRGBPixel::FromChar(from[from_y_0 + 1][from_x_0 + 1]) * from_x_frac * from_y_frac;

      const auto blend = ((P00 + P01 + P10 + P11) + (kFactor * kFactor / 2)) >> (2 * kShift);
      to[to_y][to_x] = blend.ToChar();
    }
  }
}

#endif // _CROW_FRACTAL_SERVER_IMAGE_OPERATIONS_
