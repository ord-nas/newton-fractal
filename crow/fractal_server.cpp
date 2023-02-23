#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <set>
#include <mutex>
#include <chrono>

#include <png++/png.hpp>
#include <crow.h>
#include <boost/version.hpp>

#include "fpng/fpng.h"

#include "complex.h"
#include "complex_array.h"
#include "polynomial.h"
#include "analyzed_polynomial.h"
#include "development_utils.h"
#include "fractal_params.h"
#include "thread_pool.h"

using RGBImage = png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel>>;

uint64_t Now() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

crow::query_string GetBodyParams(const crow::request& req) {
  std::string fake_url = "?" + req.body;
  // std::string fake_url = "?width=1280&height=720&zero_rs=0.28996322542741826&zero_rs=0.5620779489112948&zero_rs=0.512216828436002&zero_rs=-0.5068977187436685&zero_rs=0.920913298409193&zero_rs=-0.160837351103718&zero_is=0.5078938895349396&zero_is=-0.7409446143156879&zero_is=0.702083372851301&zero_is=-0.5798783869152273&zero_is=-0.7920066322468213&zero_is=0.356435466743505&zero_reds=177&zero_reds=185&zero_reds=182&zero_reds=187&zero_reds=70&zero_reds=73&zero_greens=224&zero_greens=214&zero_greens=39&zero_greens=36&zero_greens=88&zero_greens=103&zero_blues=235&zero_blues=1&zero_blues=201&zero_blues=132&zero_blues=114&zero_blues=119&i_min=-1.6796875&i_max=1.1328125&r_min=-2.296875&r_max=2.703125";
  return crow::query_string(fake_url);
}

std::string ParamsToString(const crow::query_string& params) {
  std::ostringstream ss;
  const auto keys = params.keys();
  const std::set<std::string> unique_keys(keys.begin(), keys.end());
  for (const std::string& key : unique_keys) {
    const std::vector<char*> values = params.get_list(key, /*use_brackets=*/false);
    if (values.size() == 1) {
      ss << "  " << key << " => " << std::string(values[0]) << std::endl;
    } else {
      ss << "  " << key << " => [" << std::endl;
      for (const char* value : values) {
	ss << "    " << std::string(value) << "," << std::endl;
      }
      ss << "  ]" << std::endl;
    }
  }
  return ss.str();
}

template <typename T>
std::vector<Complex<T>> DoubleTo(const std::vector<ComplexD>& input) {
  std::vector<Complex<T>> output;
  output.reserve(input.size());
  for (const ComplexD& in : input) {
    output.emplace_back(in.r, in.i);
  }
  return output;
}

template <typename T>
size_t NaiveDraw(const FractalParams& params, const AnalyzedPolynomial<T>& p, RGBImage& image) {
  size_t total_iters = 0;
  const T i_delta = params.r_range / params.width;
  const T r_delta = params.r_range / params.width;
  T i = params.i_min;
  for (int y = params.height - 1; y >= 0; --y) {
    T r = params.r_min;
    for (size_t x = 0; x < params.width; ++x) {
      size_t iters;
      const Complex<T> result = Newton(p, Complex<T>(r, i), params.max_iters, &iters);
      total_iters += iters;
      const size_t zero_index = ClosestZero(result, p.zeros);
      image[y][x] = params.colors[zero_index];
      r += r_delta;
    }
    i += i_delta;
  }
  return total_iters;
}

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

template <size_t N>
bool HasActivePixels(const std::array<std::optional<PixelMetadata>, N>& metadata) {
  for (size_t i = 0; i < N; ++i) {
    if (metadata[i].has_value()) {
      return true;
    }
  }
  return false;
}

template<typename T>
std::optional<size_t> GetNewtonResult(const Complex<T>& z,
				      std::optional<PixelMetadata>& metadata,
				      const AnalyzedPolynomial<T>& p,
				      size_t max_iterations) {
  if (!metadata.has_value()) {
    return std::nullopt;
  }
  ++metadata->iteration_count;
  if (metadata->iteration_count >= max_iterations) {
    return ClosestZero(z, p.zeros);
  }
  return p.GetZeroIndexIfConverged(z);
}

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

struct ImageOverlap {
  ImageRect a_region;
  ImageRect b_region;
};

struct RangeOverlap {
  size_t a_min;
  size_t b_min;
  size_t extent;
};

struct ImageDelta {
  std::optional<ImageOverlap> overlap;
  std::vector<ImageRect> b_only;
};

template <typename T, size_t N>
size_t FillImageUsingDynamicBlocks(const FractalParams& params,
				   const AnalyzedPolynomial<T>& p,
				   const ImageRect rect,
				   RGBImage& image) {
  size_t total_iters = 0;

  // Make an iterator that will walk across the requested rows of our image.
  PixelIterator<T> iter({
      .r_min = static_cast<T>(params.r_min),
      .i_min = static_cast<T>(params.i_min),
      .r_delta = static_cast<T>(params.r_range / params.width),
      .i_delta = static_cast<T>(params.r_range / params.width),
      .width = params.width,
      .height = params.height,
      .x_min = rect.x_min,
      .x_max = rect.x_max,
      .y_min = static_cast<int>(rect.y_min),
      .y_max = static_cast<int>(rect.y_max),
    });

  // Fill a block with some complex numbers.
  ComplexArray<T, N> block;
  std::array<std::optional<PixelMetadata>, N> metadata;
  for (size_t b = 0; b < N; ++b) {
    std::tie(block.rs(b), block.is(b), metadata[b]) = iter.Next();
  }

  // Keep iterating Newton's algorithm on the block, pulling in new pixels as
  // old ones finish, until there are no pixels left.
  while (!iter.Done() || HasActivePixels(metadata)) {
    // Uncomment to see what CPU we're on.
    // if (total_iters % (N * 16384) == 0) {
    //   std::cout << "[" << y_min << ", " << y_max << "): " << sched_getcpu() << std::endl;
    // }
    NewtonIter(p, &block);
    total_iters += N;
    for (size_t b = 0; b < N; ++b) {
      std::optional<size_t> zero_index =
	GetNewtonResult(block.get(b), metadata[b], p, params.max_iters);
      if (zero_index.has_value()) {
	image[metadata[b]->y][metadata[b]->x] = params.colors[*zero_index];
        std::tie(block.rs(b), block.is(b), metadata[b]) = iter.Next();
      }
    }
  }

  return total_iters;
}

template <typename T, size_t N>
size_t DynamicBlockDraw(const FractalParams& params, const AnalyzedPolynomial<T>& p, RGBImage& image) {
  const ImageRect whole_image = {
    .x_min = 0,
    .x_max = params.width,
    .y_min = 0,
    .y_max = params.height,
  };
  return FillImageUsingDynamicBlocks<T, N>(params, p, whole_image, image);
}

template <typename T, size_t N>
size_t DynamicBlockThreadedDraw(const FractalParams& params, const AnalyzedPolynomial<T>& p, RGBImage& image, ThreadPool& thread_pool) {
  constexpr size_t rows_per_task = 50; //48;
  std::mutex m;
  size_t total_iters = 0;
  for (size_t start_row = 0; start_row < params.height; start_row += rows_per_task) {
    const size_t end_row = std::min(start_row + rows_per_task, params.height);
    const ImageRect rect = {
      .x_min = 0,
      .x_max = params.width,
      .y_min = start_row,
      .y_max = end_row,
    };
    thread_pool.Queue([rect, params, p,
		       &image, &total_iters, &m]() {
      size_t iters = FillImageUsingDynamicBlocks<T, N>(params, p, rect, image);
      {
	std::scoped_lock lock(m);
	total_iters += iters;
      }
    });
  }
  thread_pool.WaitUntilDone();
  return total_iters;
}

template <typename T>
std::optional<RangeOverlap> FindRangeOverlap(T a_min, T b_min, T step, size_t num_pixels) {
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
std::optional<ImageOverlap> FindImageOverlap(const FractalParams& a, const FractalParams& b) {
  const size_t width = a.width;
  const size_t height = a.height;
  const T step = a.r_range / width;

  std::optional<RangeOverlap> r_overlap = FindRangeOverlap<T>(a.r_min, b.r_min, step, width);
  std::optional<RangeOverlap> i_overlap = FindRangeOverlap<T>(a.i_min, b.i_min, step, height);
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
ImageDelta ComputeImageDelta(const FractalParams& a, const FractalParams& b) {
  // To try to mitigate floating point math issues, we use the exact set of
  // operations that the drawing function will do. This makes things a little
  // less efficient.
  ImageDelta delta;
  delta.overlap = FindImageOverlap<T>(a, b);

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

// Can likely be optimized, but not clear if necessary.
void CopyImage(const RGBImage& from, RGBImage& to, const ImageOverlap& overlap) {
  const uint64_t start_time = Now();
  size_t from_y = overlap.a_region.y_min;
  size_t to_y = overlap.b_region.y_min;
  for (; from_y < overlap.a_region.y_max; ++from_y, ++to_y) {
    size_t from_x = overlap.a_region.x_min;
    size_t to_x = overlap.b_region.x_min;
    for (; from_x < overlap.a_region.x_max; ++from_x, ++to_x) {
      to[to_y][to_x] = from[from_y][from_x];
    }
  }
  const uint64_t end_time = Now();
  std::cout << "Copy time (ms): " << (end_time - start_time) << std::endl;
}

template <typename T, size_t N>
size_t DynamicBlockThreadedIncrementalDraw(const FractalParams& params,
					   const AnalyzedPolynomial<T>& p,
					   RGBImage& image,
					   const FractalParams& previous_params,
					   const RGBImage& previous_image,
					   ThreadPool& thread_pool) {
  if (!ParamsDifferOnlyByPanning(params, previous_params)) {
    std::cout << "BAIL INCREMENTAL :(" << std::endl;
    return DynamicBlockThreadedDraw<T, N>(params, p, image, thread_pool);
  }
  const ImageDelta delta = ComputeImageDelta<T>(previous_params, params);
  std::cout << "A Params (OLD):" << std::endl;
  std::cout << "  r_min:" << previous_params.r_min << std::endl;
  std::cout << "  i_min:" << previous_params.i_min << std::endl;
  std::cout << "  r_range:" << previous_params.r_range << std::endl;
  std::cout << "B Params (NEW):" << std::endl;
  std::cout << "  r_min:" << params.r_min << std::endl;
  std::cout << "  i_min:" << params.i_min << std::endl;
  std::cout << "  r_range:" << params.r_range << std::endl;
  std::cout << "Overlap area:" << std::endl;
  if (delta.overlap.has_value()) {
    std::cout << "  A (Old):" << std::endl;
    std::cout << "    x_min: " << delta.overlap->a_region.x_min << std::endl;
    std::cout << "    x_max: " << delta.overlap->a_region.x_max << std::endl;
    std::cout << "    y_min: " << delta.overlap->a_region.y_min << std::endl;
    std::cout << "    y_may: " << delta.overlap->a_region.y_max << std::endl;
    std::cout << "  B (New):" << std::endl;
    std::cout << "    x_min: " << delta.overlap->b_region.x_min << std::endl;
    std::cout << "    x_max: " << delta.overlap->b_region.x_max << std::endl;
    std::cout << "    y_min: " << delta.overlap->b_region.y_min << std::endl;
    std::cout << "    y_max: " << delta.overlap->b_region.y_max << std::endl;
  } else {
    std::cout << "  NONE" << std::endl;
  }
  for (const auto& only : delta.b_only) {
    std::cout << "B only:" << std::endl;
    std::cout << "  x_min: " << only.x_min << std::endl;
    std::cout << "  x_max: " << only.x_max << std::endl;
    std::cout << "  y_min: " << only.y_min << std::endl;
    std::cout << "  y_max: " << only.y_max << std::endl;
  }

  // TODO: when computation is quite expensive, this can be too many pixels per
  // task. This results in us not scheduling enough tasks, and we underutilize
  // our thread pool. Maybe we compute tasks_per_worker first, which must be an
  // integer, and then split the work into num_workers * tasks_per_worker? That
  // way, we never have idle workers. Would be better to actually estimate the
  // amount of computation required per pixel though.
  constexpr size_t desired_pixels_per_task = 50 * 2000; // TUNE
  std::mutex m;
  size_t total_iters = 0;
  if (delta.overlap.has_value()) {
    thread_pool.Queue([&previous_image, &image,
		       delta]() {
      CopyImage(previous_image, image, *delta.overlap);
    });
  }
  size_t num_tasks = 0;
  for (const auto& region : delta.b_only) {
    const size_t rows_per_task = desired_pixels_per_task / region.width();
    for (size_t start_row = region.y_min; start_row < region.y_max; start_row += rows_per_task) {
      const size_t end_row = std::min(start_row + rows_per_task, region.y_max);
      const ImageRect rect = {
	.x_min = region.x_min,
	.x_max = region.x_max,
	.y_min = start_row,
	.y_max = end_row,
      };
      thread_pool.Queue([rect, params, p,
			 &image, &total_iters, &m]() {
	size_t iters = FillImageUsingDynamicBlocks<T, N>(params, p, rect, image);
	{
	  std::scoped_lock lock(m);
	  total_iters += iters;
	}
      });
      ++num_tasks;
    }
  }
  thread_pool.WaitUntilDone();
  std::cout << "Incremental draw used " << num_tasks << " tasks" << std::endl;
  return total_iters;
}

std::string EncodeWithPngPlusPlus(RGBImage& image) {
  std::ostringstream ss;
  image.write_stream(ss);
  return ss.str();
}

std::string EncodeWithFPng(RGBImage& image) {
  const void* image_bytes = image.get_pixbuf().get_bytes().data();
  std::vector<uint8_t> encoded;
  fpng::fpng_encode_image_to_memory(image_bytes, image.get_width(), image.get_height(), /*num_channels=*/3, encoded);
  return std::string(encoded.begin(), encoded.end());
}

template <typename T>
std::string GeneratePng(const FractalParams& params, ThreadPool& thread_pool) {
  // Gross. TODO FIXME
  static std::optional<FractalParams> previous_params = std::nullopt;
  static std::unique_ptr<RGBImage> previous_image = nullptr;

  // Set up the image.
  auto image = std::make_unique<RGBImage>(params.width, params.height);

  // Figure out what polynomial we're drawing.
  const AnalyzedPolynomial<T> p = AnalyzedPolynomial<T>(DoubleTo<T>(params.zeros));
  std::cout << Now() << ": Drawing: " << p << std::endl;

  // Dispatch image generation.
  const uint64_t start_time = Now();
  size_t total_iters = 0;
  switch (params.strategy.value_or(Strategy::DYNAMIC_BLOCK_THREADED)) {
   case Strategy::NAIVE:
     total_iters = NaiveDraw<T>(params, p, *image);
     break;
   case Strategy::DYNAMIC_BLOCK:
     total_iters = DynamicBlockDraw<T, 32>(params, p, *image);
     break;
   case Strategy::DYNAMIC_BLOCK_THREADED:
     total_iters = DynamicBlockThreadedDraw<T, 32>(params, p, *image, thread_pool);
     break;
   case Strategy::DYNAMIC_BLOCK_THREADED_INCREMENTAL:
     if (!previous_params.has_value() || previous_image == nullptr) {
       std::cout << "NO BASELINE FOR INCREMENTAL :(" << std::endl;
       total_iters = DynamicBlockThreadedDraw<T, 32>(params, p, *image, thread_pool);
     } else {
       total_iters = DynamicBlockThreadedIncrementalDraw<T, 32>(params, p, *image, *previous_params, *previous_image, thread_pool);
     }
     break;
  }
  const uint64_t end_time = Now();
  std::cout << "Total iterations: " << total_iters << std::endl;
  std::cout << "Computation time (ms): " << (end_time - start_time) << std::endl;

  // Dispatch PNG encoding.
  std::string png;
  switch (params.png_encoder.value_or(PngEncoder::FPNG)) {
   case PngEncoder::PNGPP:
     png = EncodeWithPngPlusPlus(*image);
     break;
   case PngEncoder::FPNG:
     png = EncodeWithFPng(*image);
     break;
  }
  const uint64_t encode_time = Now();
  std::cout << "PNG encode time (ms): " << (encode_time - end_time) << std::endl;
  std::cout << "Total time (ms): " << (encode_time - start_time) << std::endl;
  std::cout << Now() << ": Done drawing" << std::endl;

  previous_image = std::move(image);
  previous_params = params;
  return png;
}

int main() {
  std::cout << "Using Boost "
	    << BOOST_VERSION / 100000     << "."  // major version
	    << BOOST_VERSION / 100 % 1000 << "."  // minor version
	    << BOOST_VERSION % 100                // patch level
	    << std::endl;

  fpng::fpng_init();

  // Using 8 threads (since we have 8 logical CPUs) even though there are only 4
  // physical cores. Experiments seem to show that 8 is slightly faster
  // (although not 2x faster) than 4.
  ThreadPool worker_pool(8);

  crow::SimpleApp app; //define your crow application

  // Main page.
  CROW_ROUTE(app, "/")([](){
    return crow::mustache::load_text("index.html");
  });

  // Fractal image for main page.
  CROW_ROUTE(app, "/fractal").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req){
      const auto params = GetBodyParams(req);
      std::cout << "Got the following params in request:" << std::endl;
      std::cout << ParamsToString(params);
      std::optional<FractalParams> fractal_params = FractalParams::Parse(params);
      if (!fractal_params.has_value()) {
	std::cout << "Malformed params :(" << std::endl;
	return crow::response(400);
      }

      std::string png;
      switch (fractal_params->precision.value_or(Precision::SINGLE)) {
       case Precision::SINGLE:
	 png = GeneratePng<float>(*fractal_params, worker_pool);
	 break;
       case Precision::DOUBLE:
	 png = GeneratePng<double>(*fractal_params, worker_pool);
	 break;
      }
      return crow::response("png", png);
    });

  // Test page - image cycler.
  CROW_ROUTE(app, "/image_cycler.html")([](){
    return crow::mustache::load_text("image_cycler.html");
  });

  // Actual image cycler endpoint.
  ImageCycler cycler;
  CROW_ROUTE(app, "/cyclic_image").methods(crow::HTTPMethod::POST)
    ([&](){
    return crow::response("png", cycler.Get());
  });

  // Test page - dynamic image generation.
  CROW_ROUTE(app, "/dynamic_image.html")([](){
    return crow::mustache::load_text("dynamic_image.html");
  });

  // Actual dynamic image endpoint.
  CROW_ROUTE(app, "/sample_dynamic_image.png")([](){
    return crow::response("png", GenerateSamplePng());
  });

  //set the port, set the app to run on multiple threads, and run the app
  // app.port(18080).multithreaded().run();
  app.port(18080).run();
}
