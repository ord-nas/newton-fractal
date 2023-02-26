#ifndef _CROW_FRACTAL_SERVER_IMAGE_REGIONS_
#define _CROW_FRACTAL_SERVER_IMAGE_REGIONS_

#include <vector>
#include <optional>
#include <cmath>

#include "fractal_params.h"

struct ImageRect {
  // Both ranges are half-open, e.g. [x_min, x_max).
  size_t x_min;
  size_t x_max;
  size_t y_min;
  size_t y_max;

  size_t width() const {
    return x_max - x_min;
  }

  size_t height() const {
    return y_max - y_min;
  }

  size_t CountPixels() const {
    return (x_max - x_min) * (y_max - y_min);
  }
};

// An overlap between two images, `a` and `b`.
// Represented as corresponding ImageRects in both images.
struct ImageOverlap {
  ImageRect a_region;
  ImageRect b_region;
};

// An overlap between two 1D ranges.
// Represented as corresponding min points on each range, and the range extent.
struct RangeOverlap {
  size_t a_min;
  size_t b_min;
  size_t extent;
};

// The delta between two images.
struct ImageDelta {
  // The region of overlap between the two images, if any.
  std::optional<ImageOverlap> overlap;

  // A set of rectangular regions that are unique to b.
  std::vector<ImageRect> b_only;
};

// We compute this in a very slow, incremental way to avoid floating point nonesense.
// This set of computations should assign exactly the same (r, i) values to each pixel
// that PixelIterator would, ensuring no ugly seams from floating point error.
template <typename T>
std::optional<RangeOverlap> FindPanOnlyRangeOverlap(T a_min, T b_min, T step, size_t num_pixels) {
  const T start = std::min(a_min, b_min);
  const T end = std::max(a_min, b_min);

  size_t offset = 0;
  T curr = start;
  T prev = start;
  while (offset < num_pixels && curr < end) {
    ++offset;
    prev = curr;
    curr += step;
  }
  if (offset > 0 && std::abs(end - prev) < std::abs(end - curr)) {
    --offset;
  }
  if (offset == num_pixels) {
    return std::nullopt;
  }

  if (a_min < b_min) {
    return RangeOverlap{
      .a_min = offset,
      .b_min = 0,
      .extent = (num_pixels - offset),
    };
  } else {
    return RangeOverlap{
      .a_min = 0,
      .b_min = offset,
      .extent = (num_pixels - offset),
    };
  }
}

template <typename T>
std::optional<ImageOverlap> FindPanOnlyImageOverlap(const FractalParams& a, const FractalParams& b) {
  const size_t width = a.width;
  const size_t height = a.height;
  const T step = a.r_range / width;

  std::optional<RangeOverlap> r_overlap = FindPanOnlyRangeOverlap<T>(a.r_min, b.r_min, step, width);
  std::optional<RangeOverlap> i_overlap = FindPanOnlyRangeOverlap<T>(a.i_min, b.i_min, step, height);
  if (!r_overlap.has_value() || !i_overlap.has_value()) {
    return std::nullopt;
  }

  ImageOverlap image_overlap;

  // The r/x part is pretty easy.
  image_overlap.a_region.x_min = r_overlap->a_min;
  image_overlap.a_region.x_max = r_overlap->a_min + r_overlap->extent;
  image_overlap.b_region.x_min = r_overlap->b_min;
  image_overlap.b_region.x_max = r_overlap->b_min + r_overlap->extent;

  // The i/y part is a little more subtle because the coordinate systems run in
  // opposite directions.
  image_overlap.a_region.y_max = height - i_overlap->a_min;
  image_overlap.a_region.y_min = height - i_overlap->a_min - i_overlap->extent;
  image_overlap.b_region.y_max = height - i_overlap->b_min;
  image_overlap.b_region.y_min = height - i_overlap->b_min - i_overlap->extent;

  return image_overlap;
}

template <typename T>
ImageDelta ComputePanOnlyImageDelta(const FractalParams& a, const FractalParams& b) {
  ImageDelta delta;
  delta.overlap = FindPanOnlyImageOverlap<T>(a, b);

  // If there is no overlap, then the whole image is b_only.
  if (!delta.overlap.has_value()) {
    delta.b_only.push_back({
	.x_min = 0,
	.x_max = b.width,
	.y_min = 0,
	.y_max = b.height,
      });
    return delta;
  }

  // Otherwise, compute the b_only portions.
  const ImageRect b_overlap = delta.overlap->b_region;
  if (b_overlap.x_min > 0) {
    delta.b_only.push_back({
	.x_min = 0,
	.x_max = b_overlap.x_min,
	.y_min = 0,
	.y_max = b.height,
      });
  }
  if (b_overlap.x_max < b.width) {
    delta.b_only.push_back({
	.x_min = b_overlap.x_max,
	.x_max = b.width,
	.y_min = 0,
	.y_max = b.height,
      });
  }
  if (b_overlap.y_min > 0) {
    delta.b_only.push_back({
	.x_min = b_overlap.x_min,
	.x_max = b_overlap.x_max,
	.y_min = 0,
	.y_max = b_overlap.y_min,
      });
  }
  if (b_overlap.y_max < b.height) {
    delta.b_only.push_back({
	.x_min = b_overlap.x_min,
	.x_max = b_overlap.x_max,
	.y_min = b_overlap.y_max,
	.y_max = b.height,
      });
  }
  return delta;
}

size_t TransformAndClampX(size_t pixel, size_t image_dim,
			  double from_float_range, double to_float_range,
			  double from_float_min, double to_float_min) {
  const double float_value = 1.0 * pixel / image_dim * from_float_range + from_float_min;
  const double to_pixel = (float_value - to_float_min) / to_float_range * image_dim;
  return (to_pixel < 0 ? 0 :
	  to_pixel >= image_dim ? image_dim :
	  static_cast<size_t>(to_pixel));
}

size_t TransformAndClampY(size_t pixel, size_t image_dim,
			  double from_float_range, double to_float_range,
			  double from_float_min, double to_float_min) {
  const double from_float_max = from_float_min + from_float_range;
  const double to_float_max = to_float_min + to_float_range;
  const double float_value = from_float_max - 1.0 * pixel / image_dim * from_float_range;
  const double to_pixel = (to_float_max - float_value) / to_float_range * image_dim;
  return (to_pixel < 0 ? 0 :
	  to_pixel >= image_dim ? image_dim :
	  static_cast<size_t>(to_pixel));
}

ImageRect TransformAndClamp(const ImageRect& from_rect,
			    const FractalParams& from_params,
			    const FractalParams& to_params) {
  const size_t x_min = TransformAndClampX(from_rect.x_min, from_params.width,
					  from_params.r_range, to_params.r_range,
					  from_params.r_min, to_params.r_min);
  const size_t x_max = TransformAndClampX(from_rect.x_max, from_params.width,
					  from_params.r_range, to_params.r_range,
					  from_params.r_min, to_params.r_min);
  const size_t y_min = TransformAndClampY(from_rect.y_min, from_params.height,
					  from_params.i_range(), to_params.i_range(),
					  from_params.i_min, to_params.i_min);
  const size_t y_max = TransformAndClampY(from_rect.y_max, from_params.height,
					  from_params.i_range(), to_params.i_range(),
					  from_params.i_min, to_params.i_min);
  return ImageRect{
    .x_min = x_min,
    .x_max = x_max,
    .y_min = y_min,
    .y_max = y_max,
  };
}

std::optional<ImageOverlap> FindGeneralImageOverlap(const FractalParams& a,
						    const FractalParams& b) {
  const ImageRect a_entire_image = {
    .x_min = 0,
    .x_max = a.width,
    .y_min = 0,
    .y_max = a.height,
  };
  const ImageRect b_overlap = TransformAndClamp(a_entire_image, a, b);
  const ImageRect a_overlap = TransformAndClamp(b_overlap, b, a);

  if (a_overlap.CountPixels() == 0 || b_overlap.CountPixels() == 0) {
    return std::nullopt;
  }

  return ImageOverlap{
    .a_region = a_overlap,
    .b_region = b_overlap,
  };
}

#endif // _CROW_FRACTAL_SERVER_IMAGE_REGIONS_
