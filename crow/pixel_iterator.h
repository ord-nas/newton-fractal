#ifndef _CROW_FRACTAL_SERVER_PIXEL_ITERATOR_
#define _CROW_FRACTAL_SERVER_PIXEL_ITERATOR_

#include <optional>
#include <tuple>

#include "image_regions.h"

struct PixelMetadata {
  size_t x;
  size_t y;
  size_t iteration_count;
};

template <typename T>
class PixelIterator {
 public:
  struct Options {
    T r_min;
    T i_min;
    T r_delta;
    T i_delta;
    size_t width;
    size_t height;

    // x is in range [x_min, x_max)
    size_t x_min = 0;
    size_t x_max = 0;

    // y is in range [y_min, y_max)
    // Use int inside of size_t since we *decrement* y as we iterate, meaning that
    // it's possible to it to go negative.
    int y_min = 0;
    int y_max = 0;
  };

  static void FillMissing(Options& options) {
    if (options.x_min == 0 && options.x_max == 0) {
      options.x_max = options.width;
    }
    if (options.y_min == 0 && options.y_max == 0) {
      options.y_max = options.height;
    }
  }

  PixelIterator<T>(const Options& options_in)
    : options(options_in), r_curr(options.r_min), i_curr(options.i_min),
      x(0), y(options.height - 1) {
    FillMissing(options);

    // Position x at x_min.
    for (; x < options.x_min; ++x) {
      r_curr += options.r_delta;
    }
    r_start = r_curr;

    // Position y at y_max - 1.
    for (; y >= options.y_max; --y) {
      i_curr += options.i_delta;
    }
  };

  bool Done() const {
    return y < options.y_min;
  }

  std::tuple<T, T, std::optional<PixelMetadata>> Next() {
    // Check if we're done.
    if (y < options.y_min) {
      return std::make_tuple(T(0.0), T(0.0), std::nullopt);
    }

    // Compose the next point.
    auto result = std::make_tuple(r_curr, i_curr, PixelMetadata({
	  .x = x,
	  .y = static_cast<size_t>(y),
	  .iteration_count = 0
	}));

    // Increment.
    ++x;
    r_curr += options.r_delta;
    if (x >= options.x_max) {
      x = options.x_min;
      r_curr = r_start;
      --y;
      i_curr += options.i_delta;
    }

    return result;
  }

  // Initialization parameters.
  Options options;

  // Where to reset r to each time we go to the next row.
  T r_start;

  // Current iteration state.
  T r_curr;
  T i_curr;
  size_t x;
  int y;
};

#endif // _CROW_FRACTAL_SERVER_PIXEL_ITERATOR_
