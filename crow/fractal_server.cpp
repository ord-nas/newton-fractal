#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <optional>

#include <png++/png.hpp>
#include <crow.h>

#include "complex.h"
#include "polynomial.h"
#include "analyzed_polynomial.h"

int i = 0;

bool ParsePositiveInt(const crow::query_string& url_params,
		      const std::string& key,
		      int* output) {
  const char* value = url_params.get(key);
  if (value == nullptr) {
    return false;
  }
  std::string value_str(value);

  int num = 0;
  size_t chars_processed = 0;
  try {
    num = std::stoi(value_str, &chars_processed);
  } catch (std::invalid_argument const& ex) {
    return false;
  } catch(std::out_of_range const& ex) {
    return false;
  }

  if (chars_processed != value_str.size()) {
    return false;
  }

  if (num <= 0) {
    return false;
  }

  *output = num;
  return true;
}

bool ParseFiniteDouble(const crow::query_string& url_params,
		       const std::string& key,
		       double* output) {
  const char* value = url_params.get(key);
  if (value == nullptr) {
    return false;
  }
  std::string value_str(value);

  double num = 0;
  size_t chars_processed = 0;
  try {
    num = std::stod(value_str, &chars_processed);
  } catch (std::invalid_argument const& ex) {
    return false;
  } catch(std::out_of_range const& ex) {
    return false;
  }

  if (chars_processed != value_str.size()) {
    return false;
  }

  if (!std::isfinite(num)) {
    return false;
  }

  *output = num;
  return true;
}

struct FractalParams {
  static std::optional<FractalParams> Parse(const crow::query_string& url_params) {
    FractalParams fractal_params;
    if (ParseFiniteDouble(url_params, "i_min", &fractal_params.i_min) &&
	ParseFiniteDouble(url_params, "i_max", &fractal_params.i_max) &&
	ParseFiniteDouble(url_params, "r_min", &fractal_params.r_min) &&
	ParseFiniteDouble(url_params, "r_max", &fractal_params.r_max) &&
	ParsePositiveInt(url_params, "width", &fractal_params.width) &&
	ParsePositiveInt(url_params, "height", &fractal_params.height)) {
      return fractal_params;
    }
    return std::nullopt;
  }

  double i_min;
  double i_max;
  double r_min;
  double r_max;
  int width;
  int height;
};

crow::query_string GetBodyParams(const crow::request& req) {
  std::string fake_url = "?" + req.body;
  return crow::query_string(fake_url);
}

std::string DrawToPng(const FractalParams& params) {
  png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel>>
    image(params.width, params.height);

  const Complex origin(0.0, 0.0);
  const double r_rng = 5.0;
  const double i_rng = r_rng / params.width * params.height;
  const double r_min = origin.r - r_rng / 2;
  const double r_max = origin.r + r_rng / 2;
  const double i_min = origin.i - i_rng / 2;
  const double i_max = origin.i + i_rng / 2;

  std::random_device rd;
  std::mt19937 twister(rd());
  std::uniform_real_distribution<> dist(-1.0, 1.0);
  const AnalyzedPolynomial p = AnalyzedPolynomial({Complex(dist(twister), dist(twister)),
						   Complex(dist(twister), dist(twister)),
						   Complex(dist(twister), dist(twister))});
  // const AnalyzedPolynomial p = AnalyzedPolynomial({Complex(1.0, 0.0),
  // 						   Complex(-0.5, 0.86602540378),
  // 						   Complex(-0.5, -0.86602540378)});
  std::cout << "Drawing: " << p << std::endl;

  const double i_delta = (i_max - i_min) / params.height;
  const double r_delta = (r_max - r_min) / params.width;
  double i = i_min;
  for (size_t y = 0; y < params.height; ++y) {
    double r = r_min;
    for (size_t x = 0; x < params.width; ++x) {
      const Complex result = Newton(p, Complex(r, i), 100);
      const size_t zero_index = ClosestZero(result, p.zeros);
      if (zero_index == 0) {
	image[y][x] = png::rgb_pixel(255, 0, 0);
      } else if (zero_index == 1) {
        image[y][x] = png::rgb_pixel(0, 255, 0);
      } else if (zero_index == 2) {
	image[y][x] = png::rgb_pixel(0, 0, 255);
      }
      r += r_delta;
    }
    i += i_delta;
  }

  std::ostringstream png_sstrm;
  image.write_stream(png_sstrm);
  return png_sstrm.str();
}

int main() {
  std::vector<std::string> images;
  for (int img = 0; img < 5; img++) {
    const char c = 'a' + img;
    const std::string filename = "images/" + std::string(1, c) + "_small.png";
    std::ifstream fin(filename,  std::ios::binary);
    std::ostringstream sstrm;
    sstrm << fin.rdbuf();
    images.push_back(sstrm.str());
  }

  // Dynamically generate an in-memory PNG.
  png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel>> image(1280, 720);
  for (png::uint_32 y = 0; y < image.get_height(); ++y) {
    for (png::uint_32 x = 0; x < image.get_width(); ++x) {
      image[y][x] = png::rgb_pixel(x, y, x + y);
    }
  }
  std::ostringstream png_sstrm;
  image.write_stream(png_sstrm);
  images.push_back(png_sstrm.str());

  Polynomial p1({Complex(1, 0), Complex(-2, 0)});
  Polynomial p2({Complex(5, 0), Complex(3, 0)});
  std::cout << p1 << " * " << p2 << " = " << (p1 * p2) << std::endl;

  AnalyzedPolynomial ap({Complex(1, 2), Complex(3, 4), Complex(5, 6)});
  std::cout << ap << std::endl;

  crow::SimpleApp app; //define your crow application

  //define your endpoint at the root directory
  CROW_ROUTE(app, "/")([](){
    std::cout << "Main page okay!";
    auto page = crow::mustache::load_text("index.html");
    return page;
  });

  //define dynamic image endpoint
  CROW_ROUTE(app, "/dynamic_image").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req){
      std::cout << "Got the following params in request:" << std::endl;
      const auto& params = GetBodyParams(req);
      for (const std::string& key : params.keys()) {
	std::cout << "  " << key << " => " << std::string(params.get(key)) << std::endl;
      }
      std::optional<FractalParams> fractal_params = FractalParams::Parse(params);
      if (!fractal_params.has_value()) {
	std::cout << "Malformed params :(" << std::endl;
	return crow::response(400);
      } else {
	return crow::response("png", DrawToPng(*fractal_params));
      }
      // i++;
      // i = i % (images.size() + 1);
      // if (i >= images.size()) {
      //   // The real deal!
      //   return crow::response("png", DrawToPng(1280, 720));
      // } else {
      //   return crow::response("png", images[i]);
      // }
    });

  //set the port, set the app to run on multiple threads, and run the app
  app.port(18080).multithreaded().run();
}
