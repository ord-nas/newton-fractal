#ifndef _CROW_FRACTAL_SERVER_FRACTAL_DRAWING_
#define _CROW_FRACTAL_SERVER_FRACTAL_DRAWING_

#include <vector>
#include <optional>

#include "rgb_image.h"
#include "complex.h"
#include "complex_array.h"
#include "polynomial.h"
#include "analyzed_polynomial.h"
#include "fractal_params.h"
#include "thread_pool.h"
#include "task_group.h"
#include "image_regions.h"
#include "image_operations.h"
#include "pixel_iterator.h"
#include "development_utils.h"

template <typename T>
std::vector<Complex<T>> DoubleTo(const std::vector<ComplexD>& input) {
  std::vector<Complex<T>> output;
  output.reserve(input.size());
  for (const ComplexD& in : input) {
    output.emplace_back(in.r, in.i);
  }
  return output;
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

template <size_t N>
bool HasActivePixels(const std::array<std::optional<PixelMetadata>, N>& metadata) {
  for (size_t i = 0; i < N; ++i) {
    if (metadata[i].has_value()) {
      return true;
    }
  }
  return false;
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


template <typename T, size_t N>
size_t FillRegionUsingDynamicBlocks(const FractalParams& params,
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
  return FillRegionUsingDynamicBlocks<T, N>(params, p, whole_image, image);
}

template <typename T, size_t N>
size_t DynamicBlockThreadedDraw(const FractalParams& params, const AnalyzedPolynomial<T>& p, RGBImage& image, ThreadPool& thread_pool) {
  TaskGroup task_group(&thread_pool);
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
    task_group.Add([rect, params, p,
		    &image, &total_iters, &m]() {
      size_t iters = FillRegionUsingDynamicBlocks<T, N>(params, p, rect, image);
      std::scoped_lock lock(m);
      total_iters += iters;
    });
  }
  task_group.WaitUntilDone();
  return total_iters;
}

template <typename T, size_t N>
size_t DynamicBlockThreadedIncrementalDraw(const FractalParams& params,
					   const AnalyzedPolynomial<T>& p,
					   RGBImage& image,
					   ThreadPool& thread_pool,
					   const std::optional<FractalParams>& previous_params,
					   const RGBImage* previous_image) {
  if (!previous_params.has_value() || previous_image == nullptr ||
      !ParamsDifferOnlyByPanning(params, *previous_params)) {
    return DynamicBlockThreadedDraw<T, N>(params, p, image, thread_pool);
  }

  const ImageDelta delta = ComputePanOnlyImageDelta<T>(*previous_params, params);
  TaskGroup task_group(&thread_pool);
  if (delta.overlap.has_value()) {
    task_group.Add([previous_image, &image, delta]() {
      const uint64_t start_time = Now();
      CopyImage(*previous_image, image, *delta.overlap);
      const uint64_t end_time = Now();
      std::cout << "Copy time (ms): " << (end_time - start_time) << std::endl;
    });
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
      task_group.Add([rect, params, p,
		      &image, &total_iters, &m]() {
	size_t iters = FillRegionUsingDynamicBlocks<T, N>(params, p, rect, image);
	std::scoped_lock lock(m);
	total_iters += iters;
      });
      ++num_tasks;
    }
  }
  task_group.WaitUntilDone();
  std::cout << "Incremental draw used " << num_tasks << " tasks" << std::endl;
  return total_iters;
}

struct DrawFractalArgs {
  const FractalParams& params;
  RGBImage& image;

  const std::optional<FractalParams>& previous_params;
  const RGBImage* previous_image;

  ThreadPool& thread_pool;
};

template <typename T>
size_t DrawFractalImpl(const DrawFractalArgs& args) {
  // Figure out what polynomial we're drawing.
  const AnalyzedPolynomial<T> p = AnalyzedPolynomial<T>(DoubleTo<T>(args.params.zeros));
  std::cout << "Drawing: " << p << std::endl;

  // Dispatch image generation.
  size_t total_iters = 0;
  switch (args.params.strategy.value_or(Strategy::DYNAMIC_BLOCK_THREADED_INCREMENTAL)) {
    case Strategy::NAIVE:
      total_iters = NaiveDraw<T>(args.params, p, args.image);
      break;
    case Strategy::DYNAMIC_BLOCK:
      total_iters = DynamicBlockDraw<T, 32>(args.params, p, args.image);
      break;
    case Strategy::DYNAMIC_BLOCK_THREADED:
      total_iters = DynamicBlockThreadedDraw<T, 32>(
          args.params, p, args.image, args.thread_pool);
      break;
    case Strategy::DYNAMIC_BLOCK_THREADED_INCREMENTAL:
      total_iters = DynamicBlockThreadedIncrementalDraw<T, 32>(
          args.params, p, args.image, args.thread_pool, args.previous_params, args.previous_image);
      break;
  }

  return total_iters;
}

size_t DrawFractal(const DrawFractalArgs& args) {
  size_t total_iters = 0;
  switch (args.params.precision.value_or(Precision::SINGLE)) {
    case Precision::SINGLE:
      total_iters = DrawFractalImpl<float>(args);
      break;
    case Precision::DOUBLE:
      total_iters = DrawFractalImpl<double>(args);
      break;
  }
  return total_iters;
}

#endif // _CROW_FRACTAL_SERVER_FRACTAL_DRAWING_
