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
#include "coordinator.h"
#include "image_regions.h"
#include "pixel_iterator.h"

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
size_t DynamicBlockThreadedDraw(const FractalParams& params, const AnalyzedPolynomial<T>& p, RGBImage& image, Coordinator& coordinator) {
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
    coordinator.QueueComputation([rect, params, p,
				  &image, &total_iters, &m]() {
      size_t iters = FillImageUsingDynamicBlocks<T, N>(params, p, rect, image);
      std::scoped_lock lock(m);
      total_iters += iters;
    });
  }
  coordinator.WaitUntilComputationDone();
  return total_iters;
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
					   Coordinator& coordinator,
					   const std::optional<FractalParams>& previous_params,
					   const RGBImage* previous_image) {
  if (!previous_params.has_value() || previous_image == nullptr ||
      !ParamsDifferOnlyByPanning(params, *previous_params)) {
    return DynamicBlockThreadedDraw<T, N>(params, p, image, coordinator);
  }
  const ImageDelta delta = ComputeImageDelta<T>(*previous_params, params);

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
    coordinator.QueueComputation([previous_image, &image, delta]() {
      CopyImage(*previous_image, image, *delta.overlap);
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
      coordinator.QueueComputation([rect, params, p,
				    &image, &total_iters, &m]() {
	size_t iters = FillImageUsingDynamicBlocks<T, N>(params, p, rect, image);
	std::scoped_lock lock(m);
	total_iters += iters;
      });
      ++num_tasks;
    }
  }
  coordinator.WaitUntilComputationDone();
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
std::string GeneratePng(const FractalParams& params, Coordinator& coordinator) {
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
     total_iters = DynamicBlockThreadedDraw<T, 32>(params, p, *image, coordinator);
     break;
   case Strategy::DYNAMIC_BLOCK_THREADED_INCREMENTAL:
     total_iters = DynamicBlockThreadedIncrementalDraw<T, 32>(
         params, p, *image, coordinator, previous_params, previous_image.get());
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

crow::response ImageWithMetadata(std::string png_contents,
				 const crow::json::wvalue& metadata) {

  crow::multipart::part png_part;
  png_part.body = std::move(png_contents);
  {
    crow::multipart::header content_type_header;
    content_type_header.value = "image/png";
    crow::multipart::header content_disposition_header;
    content_disposition_header.value = "form-data";
    content_disposition_header.params["name"] = "fractal_image";
    content_disposition_header.params["filename"] = "fractal_image.png";
    png_part.headers.emplace("Content-Type", content_type_header);
    png_part.headers.emplace("Content-Disposition", content_disposition_header);
  }

  crow::multipart::part metadata_part;
  metadata_part.body = metadata.dump();
  {
    crow::multipart::header content_type_header;
    content_type_header.value = "application/json";
    crow::multipart::header content_disposition_header;
    content_disposition_header.value = "form-data";
    content_disposition_header.params["name"] = "metadata";
    metadata_part.headers.emplace("Content-Type", content_type_header);
    metadata_part.headers.emplace("Content-Disposition", content_disposition_header);
  }

  return crow::response(crow::multipart::message(/*header=*/{},
						 /*boudary=*/"CROW-BOUNDARY",
						 /*parts=*/{png_part, metadata_part}));
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
  Coordinator coordinator(/*num_threads=*/8);

  crow::SimpleApp app; //define your crow application

  // Main page.
  CROW_ROUTE(app, "/")([](){
    return crow::mustache::load_text("index.html");
  });

  // Params endpoint.
  CROW_ROUTE(app, "/params").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req){
      const auto params = GetBodyParams(req);
      // std::cout << "Got the following params in request:" << std::endl;
      // std::cout << ParamsToString(params);
      std::optional<FractalParams> fractal_params = FractalParams::Parse(params);
      if (!fractal_params.has_value()) {
	std::cout << "Malformed params :(" << std::endl;
	return crow::response(400);
      }
      crow::json::wvalue json({{"request_id", fractal_params->request_id}});
      return crow::response(json);
    });

  // Fractal image for main page.
  CROW_ROUTE(app, "/fractal").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req){
      const auto params = GetBodyParams(req);
      // std::cout << "Got the following params in request:" << std::endl;
      // std::cout << ParamsToString(params);
      std::optional<FractalParams> fractal_params = FractalParams::Parse(params);
      if (!fractal_params.has_value()) {
	std::cout << "Malformed params :(" << std::endl;
	return crow::response(400);
      }

      std::string png;
      switch (fractal_params->precision.value_or(Precision::SINGLE)) {
       case Precision::SINGLE:
	 png = GeneratePng<float>(*fractal_params, coordinator);
	 break;
       case Precision::DOUBLE:
	 png = GeneratePng<double>(*fractal_params, coordinator);
	 break;
      }

      return ImageWithMetadata(std::move(png),
			       {{"data_id", fractal_params->request_id},
				{"viewport_id", fractal_params->request_id}});
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
  app.port(18080).multithreaded().run();
  //app.port(18080).run();
}
