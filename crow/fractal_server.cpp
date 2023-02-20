#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <set>

#include <png++/png.hpp>
#include <crow.h>

#include "complex.h"
#include "complex_array.h"
#include "polynomial.h"
#include "analyzed_polynomial.h"
#include "development_utils.h"
#include "fractal_params.h"

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
std::string DrawToPng(const FractalParams& params) {
  size_t total_iters = 0;

  png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel>>
    image(params.width, params.height);
  const AnalyzedPolynomial<T> p = AnalyzedPolynomial<T>(DoubleTo<T>(params.zeros));
  std::cout << "Drawing: " << p << std::endl;

  const T i_delta = (params.i_max - params.i_min) / params.height;
  const T r_delta = (params.r_max - params.r_min) / params.width;
  T i = params.i_min;
  for (int y = params.height - 1; y >= 0; --y) {
    T r = params.r_min;
    for (size_t x = 0; x < params.width; ++x) {
      size_t iters;
      const Complex<T> result = Newton(p, Complex<T>(r, i), 100, &iters);
      total_iters += iters;
      const size_t zero_index = ClosestZero(result, p.zeros);
      image[y][x] = params.colors[zero_index];
      r += r_delta;
    }
    i += i_delta;
  }

  std::cout << "Total iterations with vanilla algo: " << total_iters << std::endl;
  std::ostringstream png_sstrm;
  image.write_stream(png_sstrm);
  return png_sstrm.str();
}

template <typename T, size_t N>
std::array<T, N> GetBlockValues(T start, T inc) {
  std::array<T, N> values;
  values[0] = start;
  for (size_t i = 1; i < N; ++i) {
    values[i] = values[i-1] + inc;
  }
  return values;
}

template <typename T, size_t N>
ComplexArray2<T, N*N> GetBlock(const std::array<T, N>& rs,
			       const std::array<T, N>& is) {
  ComplexArray2<T, N*N> block;
  for (size_t i = 0; i < N*N; ++i) {
    block.rs[i] = rs[i % N];
    block.is[i] = is[i / N];
  }
  return block;
}

template <typename T, size_t N>
T MultByAdding(T x) {
  T result = 0;
  for (size_t i = 0; i < N; ++i) {
    result += x;
  }
  return result;
}

template <size_t N>
std::string BlockDrawToPng(const FractalParams& params) {
  size_t total_iters = 0;

  // Ensure divisibility by block_dim.
  size_t width = params.width / N * N;
  size_t height = params.height / N * N;
  png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel>>
    image(width, height);
  const AnalyzedPolynomialD p = AnalyzedPolynomialD(params.zeros);
  std::cout << "Drawing: " << p << std::endl;

  using BlockT = ComplexArray2<double, N * N>;
  const double i_little_delta = (params.i_max - params.i_min) / height;
  const double r_little_delta = (params.r_max - params.r_min) / width;
  const double i_big_delta = MultByAdding<double, N>(i_little_delta);
  const double r_big_delta = MultByAdding<double, N>(r_little_delta);
  double i = params.i_min;
  for (int y = height - 1; y >= 0; y -= N) {
    // std::cout << "block_is" << std::endl;
    const std::array<double, N> block_is = GetBlockValues<double, N>(i, i_little_delta);
    double r = params.r_min;
    for (size_t x = 0; x < width; x += N) {
      // std::cout << "block_rs" << std::endl;
      const std::array<double, N> block_rs = GetBlockValues<double, N>(r, r_little_delta);
      // std::cout << "get_block" << std::endl;
      BlockT block = GetBlock<double, N>(block_rs, block_is);
      // std::cout << "netwon" << std::endl;
      size_t iters;
      const BlockT result = Newton(p, block, 100, &iters);
      total_iters += N*N*iters;
      // std::cout << "post" << std::endl;
      for (size_t block_offset = 0; block_offset < N*N; ++block_offset) {
	// std::cout << "closest_zero" << std::endl;
	const size_t zero_index = ClosestZero(result.get(block_offset), p.zeros);
	// std::cout << "set_pixel" << std::endl;
	image[y - block_offset / N][x + block_offset % N] = params.colors[zero_index];
      }
      // std::cout << "done inner iter" << std::endl;
      r += r_big_delta;
    }
    // std::cout << "done outer iter" << std::endl;
    i += i_big_delta;
  }

  std::cout << "Total iterations with block algo: " << total_iters << std::endl;
  std::ostringstream png_sstrm;
  image.write_stream(png_sstrm);
  return png_sstrm.str();
}

struct PixelMetadata {
  size_t x;
  size_t y;
  size_t iteration_count;
};

template <typename T>
class PixelIterator {
 public:
  PixelIterator<T>(T r_min, T i_min, T r_delta, T i_delta, size_t width, size_t height)
    : r_min(r_min), i_min(i_min), r_delta(r_delta), i_delta(i_delta), width(width),
      r_curr(r_min), i_curr(i_min), x(0), y(height - 1) {};

  bool Done() const {
    return y < 0;
  }

  std::tuple<T, T, std::optional<PixelMetadata>> Next() {
    // Check if we're done.
    if (y < 0) {
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
    r_curr += r_delta;
    if (x >= width) {
      x = 0;
      r_curr = r_min;
      --y;
      i_curr += i_delta;
    }

    return result;
  }

  // Initialization parameters.
  T r_min;
  T i_min;
  T r_delta;
  T i_delta;
  size_t width;

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

template <typename T, size_t N>
std::string DynamicBlockDrawToPng(const FractalParams& params) {
  size_t total_iters = 0;

  png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel>>
    image(params.width, params.height);
  const AnalyzedPolynomial<T> p = AnalyzedPolynomial<T>(DoubleTo<T>(params.zeros));
  std::cout << "Drawing: " << p << std::endl;

  // Make an iterator that will walk across our image.
  const T i_delta = (params.i_max - params.i_min) / params.height;
  const T r_delta = (params.r_max - params.r_min) / params.width;
  PixelIterator<T> iter(params.r_min, params.i_min,
			r_delta, i_delta,
			params.width, params.height);

  // Fill a block with some complex numbers.
  ComplexArray2<T, N> block;
  std::array<std::optional<PixelMetadata>, N> metadata;
  for (size_t b = 0; b < N; ++b) {
    std::tie(block.rs[b], block.is[b], metadata[b]) = iter.Next();
  }

  // Keep iterating Newton's algorithm on the block, pulling in new pixels as
  // old ones finish, until there are no pixels left.
  while (!iter.Done() || HasActivePixels(metadata)) {
    NewtonIter(p, &block);
    total_iters += N;
    for (size_t b = 0; b < N; ++b) {
      std::optional<size_t> zero_index =
	GetNewtonResult(block.get(b), metadata[b], p, 100);
      if (zero_index.has_value()) {
	image[metadata[b]->y][metadata[b]->x] = params.colors[*zero_index];
        std::tie(block.rs[b], block.is[b], metadata[b]) = iter.Next();
      }
    }
  }

  std::cout << "Total iterations with dynamic block algo: " << total_iters << std::endl;
  std::ostringstream png_sstrm;
  image.write_stream(png_sstrm);
  return png_sstrm.str();
}


template <size_t N>
void DoTest() {
  {
    ComplexArray<double, N> a;
    ComplexArray<double, N> b;
    for (size_t i = 0; i < N; ++i) {
      a.values[i] = ComplexD(i, i+1);
      b.values[i] = ComplexD(i+2, i+3);
    }
    a += b;
    for (size_t i = 0; i < N; ++i) {
      std::cout << i << ": " << a.values[i] << std::endl;
    }
  }

  {
    ComplexArray2<double, N> a;
    ComplexArray2<double, N> b;
    for (size_t i = 0; i < N; ++i) {
      a.rs[i] = i;
      a.is[i] = i+1;
      b.rs[i] = i+2;
      b.is[i] = i+3;
    }
    a += b;
    for (size_t i = 0; i < N; ++i) {
      std::cout << i << ": " << ComplexD(a.rs[i], a.is[i]) << std::endl;
    }
  }
}

// template <size_t N>
// void DoTest2() {
//   const AnalyzedPolynomialD p = AnalyzedPolynomialD({
//       ComplexD(1.0, 0.0),
//       ComplexD(-0.5, 0.86602540378),
//       ComplexD(-0.5, -0.86602540378)});
//   ComplexArray<double, N> ca;
//   for (size_t i = 0; i < N; ++i) {
//     ca.values[i] = ComplexD(i, i+1);
//   }

//   ComplexArray<double, N> res = p.polynomial(ca);
//   std::cout << "Poly: " << p << std::endl;
//   for (size_t i = 0; i < N; ++i) {
//     std::cout << "p(" << ca.values[i] << ") = " << res.values[i] << std::endl;
//   }
// }

template <size_t N>
void DoTest3() {
  const AnalyzedPolynomialF p = AnalyzedPolynomialF({
      ComplexF(1.0, 0.0),
      ComplexF(-0.5, 0.86602540378),
      ComplexF(-0.5, -0.86602540378)});
  ComplexArray2<float, N> ca;
  for (size_t i = 0; i < N; ++i) {
    ca.rs[i] = i;
    ca.is[i] = i+1;
  }

  ComplexArray2<float, N> res = p.polynomial(ca);
  std::cout << "Poly: " << p << std::endl;
  for (size_t i = 0; i < N; ++i) {
    std::cout << "p(" << ComplexF(ca.rs[i], ca.is[i]) << ") = " << ComplexF(res.rs[i], res.is[i]) << std::endl;
  }
}

template <size_t N>
void DoTest4() {
  const AnalyzedPolynomialF p = AnalyzedPolynomialF({
      ComplexF(1.0, 0.0),
      ComplexF(-0.5, 0.86602540378),
      ComplexF(-0.5, -0.86602540378)});
  ComplexArray2<float, N> ca;
  for (size_t i = 0; i < N; ++i) {
    ca.rs[i] = i;
    ca.is[i] = i+1;
  }

  ComplexArray2<float, N> res = p(ca);
  std::cout << "Poly: " << p << std::endl;
  for (size_t i = 0; i < N; ++i) {
    std::cout << "p(" << ComplexF(ca.rs[i], ca.is[i]) << ") = " << ComplexF(res.rs[i], res.is[i]) << std::endl;
  }

  ComplexArray2<float, N> ns = Newton(p, ca, 100);
  for (size_t i = 0; i < N; ++i) {
    std::cout << "newton(p, " << ComplexF(ca.rs[i], ca.is[i]) << ") = " << ComplexF(ns.rs[i], ns.is[i]) << std::endl;
  }
}

int main() {
  DoTest<4>();

  // DoTest2<16>();
  DoTest3<16>();
  DoTest4<16>();

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
      } else {
	return crow::response("png", DrawToPng<double>(*fractal_params));
      }
    });
  CROW_ROUTE(app, "/fractal_float").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req){
      const auto params = GetBodyParams(req);
      std::cout << "Got the following params in request:" << std::endl;
      std::cout << ParamsToString(params);
      std::optional<FractalParams> fractal_params = FractalParams::Parse(params);
      if (!fractal_params.has_value()) {
	std::cout << "Malformed params :(" << std::endl;
	return crow::response(400);
      } else {
	return crow::response("png", DrawToPng<float>(*fractal_params));
      }
    });
  CROW_ROUTE(app, "/fractal_block").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req){
      const auto params = GetBodyParams(req);
      std::cout << "Got the following params in request:" << std::endl;
      std::cout << ParamsToString(params);
      std::optional<FractalParams> fractal_params = FractalParams::Parse(params);
      if (!fractal_params.has_value()) {
	std::cout << "Malformed params :(" << std::endl;
	return crow::response(400);
      } else {
	return crow::response("png", BlockDrawToPng<2>(*fractal_params));
      }
    });
  CROW_ROUTE(app, "/fractal_dynamic_block").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req){
      const auto params = GetBodyParams(req);
      std::cout << "Got the following params in request:" << std::endl;
      std::cout << ParamsToString(params);
      std::optional<FractalParams> fractal_params = FractalParams::Parse(params);
      if (!fractal_params.has_value()) {
	std::cout << "Malformed params :(" << std::endl;
	return crow::response(400);
      } else {
	return crow::response("png", DynamicBlockDrawToPng<double, 32>(*fractal_params));
      }
    });
  CROW_ROUTE(app, "/fractal_dynamic_block_float").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req){
      const auto params = GetBodyParams(req);
      std::cout << "Got the following params in request:" << std::endl;
      std::cout << ParamsToString(params);
      std::optional<FractalParams> fractal_params = FractalParams::Parse(params);
      if (!fractal_params.has_value()) {
	std::cout << "Malformed params :(" << std::endl;
	return crow::response(400);
      } else {
	return crow::response("png", DynamicBlockDrawToPng<float, 32>(*fractal_params));
      }
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
}
